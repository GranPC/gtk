/* GTK - The GIMP Toolkit
 * Copyright (C) 2023 Alice Mikhaylenko <alicem@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcssreferencevalueprivate.h"

#include "gtkcssarrayvalueprivate.h"
#include "gtkcsscustompropertypoolprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssunsetvalueprivate.h"
#include "gtkcssvalueprivate.h"
#include "gtkstyleproviderprivate.h"

#define MAX_TOKEN_LENGTH 65536

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE

  GtkStyleProperty *property;
  GtkCssVariableValue *value;
  GFile *file;
  guint subproperty;
};

static void
gtk_css_value_reference_free (GtkCssValue *value)
{
  gtk_css_variable_value_unref (value->value);
  if (value->file)
    g_object_unref (value->file);
}

static gboolean
resolve_references_do (GtkCssVariableValue *value,
                       GtkCssStyle         *style,
                       gboolean             root,
                       GPtrArray           *refs,
                       gsize               *out_length,
                       gsize               *out_n_refs)
{
  GtkCssCustomPropertyPool *pool = gtk_css_custom_property_pool_get ();
  gsize i;
  gsize length = value->length;
  gsize n_refs = 0;

  if (!root)
    {
      n_refs += 1;
      g_ptr_array_add (refs, value);
    }

  for (i = 0; i < value->n_references; i++)
    {
      GtkCssVariableValueReference *ref = &value->references[i];
      int id = gtk_css_custom_property_pool_add (pool, ref->name);
      GtkCssVariableValue *var_value;
      gsize var_length, var_refs;

      var_value = gtk_css_style_get_custom_property (style, id);

      if (!var_value || !resolve_references_do (var_value, style, FALSE, refs, &var_length, &var_refs))
        {
          var_value = ref->fallback;

          if (!var_value || !resolve_references_do (var_value, style, FALSE, refs, &var_length, &var_refs))
            goto error;
        }

      length += var_length - ref->length;
      n_refs += var_refs;

      if (length > MAX_TOKEN_LENGTH)
        goto error;
    }

  if (out_length)
    *out_length = length;

  if (out_n_refs)
    *out_n_refs = n_refs;

  return TRUE;

error:
  /* Remove the references we added as if nothing happened */
  g_ptr_array_remove_range (refs, refs->len - n_refs, n_refs);

  if (out_length)
    *out_length = 0;

  if (out_n_refs)
    *out_n_refs = 0;

  return FALSE;
}

static GtkCssVariableValue **
resolve_references (GtkCssVariableValue *input,
                    GtkCssStyle         *style,
                    gsize               *n_refs)
{
  GPtrArray *refs = g_ptr_array_new ();

  if (!resolve_references_do (input, style, TRUE, refs, NULL, NULL))
    return NULL;

  return (GtkCssVariableValue **) g_ptr_array_steal (refs, n_refs);
}

static void
parser_error (GtkCssParser         *parser,
              const GtkCssLocation *start,
              const GtkCssLocation *end,
              const GError         *error,
              gpointer              user_data)
{
  GtkStyleProvider *provider = user_data;
  GtkCssSection *section;

  section = gtk_css_section_new (gtk_css_parser_get_file (parser),
                                 start,
                                 end);

  gtk_style_provider_emit_error (provider, section, (GError *) error);

  gtk_css_section_unref (section);
}

static GtkCssValue *
gtk_css_value_reference_compute (GtkCssValue      *value,
                                 guint             property_id,
                                 GtkStyleProvider *provider,
                                 GtkCssStyle      *style,
                                 GtkCssStyle      *parent_style)
{
  GtkCssValue *result = NULL, *computed;
  GtkCssVariableValue **refs;
  gsize n_refs = 0;

  refs = resolve_references (value->value, style, &n_refs);

  if (refs != NULL)
    {
      GtkCssParser *value_parser =
        gtk_css_parser_new_for_token_stream (value->value,
                                             value->file,
                                             refs,
                                             n_refs,
                                             parser_error, provider, NULL);
      // TODO: cache parser

      result = _gtk_style_property_parse_value (value->property, value_parser);
      gtk_css_parser_unref (value_parser);
    }

  if (result == NULL)
    result = _gtk_css_unset_value_new ();

  if (GTK_IS_CSS_SHORTHAND_PROPERTY (value->property))
    {
      GtkCssValue *sub = gtk_css_value_ref (_gtk_css_array_value_get_nth (result, value->subproperty));
      gtk_css_value_unref (result);
      result = sub;
    }

  computed = _gtk_css_value_compute (result,
                                     property_id,
                                     provider,
                                     style,
                                     parent_style);

  gtk_css_value_unref (result);

  return computed;
}

static gboolean
gtk_css_value_reference_equal (const GtkCssValue *value1,
                               const GtkCssValue *value2)
{
  return FALSE;
}

static GtkCssValue *
gtk_css_value_reference_transition (GtkCssValue *start,
                                    GtkCssValue *end,
                                    guint        property_id,
                                    double       progress)
{
  return NULL;
}

static void
gtk_css_value_reference_print (const GtkCssValue *value,
                               GString           *string)
{
  gtk_css_variable_value_print (value->value, string);
}

static const GtkCssValueClass GTK_CSS_VALUE_REFERENCE = {
  "GtkCssReferenceValue",
  gtk_css_value_reference_free,
  gtk_css_value_reference_compute,
  gtk_css_value_reference_equal,
  gtk_css_value_reference_transition,
  NULL,
  NULL,
  gtk_css_value_reference_print
};

GtkCssValue *
_gtk_css_reference_value_new (GtkStyleProperty    *property,
                              GtkCssVariableValue *value,
                              GFile               *file)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_REFERENCE);
  result->property = property;
  result->value = gtk_css_variable_value_ref (value);

  if (file)
    result->file = g_object_ref (file);
  else
    result->file = NULL;

  return result;
}

void
_gtk_css_reference_value_set_subproperty (GtkCssValue *value,
                                          guint        property)
{
  g_assert (GTK_IS_CSS_SHORTHAND_PROPERTY (value->property));

  value->subproperty = property;
}
