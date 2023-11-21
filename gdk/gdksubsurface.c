/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2023 Red Hat, Inc.
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

#include "gdksubsurfaceprivate.h"
#include "gdksurfaceprivate.h"
#include "gdktexture.h"

enum {
  PROP_DMABUF_FORMATS = 1,
  N_PROPERTIES
};

G_DEFINE_TYPE (GdkSubsurface, gdk_subsurface, G_TYPE_OBJECT)

static void
gdk_subsurface_init (GdkSubsurface *self)
{
}

static void
gdk_subsurface_finalize (GObject *object)
{
  GdkSubsurface *subsurface = GDK_SUBSURFACE (object);

  g_ptr_array_remove (subsurface->parent->subsurfaces, subsurface);
  g_clear_object (&subsurface->parent);
  g_clear_pointer (&subsurface->dmabuf_formats, gdk_dmabuf_formats_unref);

  G_OBJECT_CLASS (gdk_subsurface_parent_class)->finalize (object);
}

static void
gdk_subsurface_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GdkSubsurface *subsurface = GDK_SUBSURFACE (object);

  switch (prop_id)
    {
    case PROP_DMABUF_FORMATS:
      g_value_set_boxed (value, subsurface->dmabuf_formats);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_subsurface_class_init (GdkSubsurfaceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gdk_subsurface_finalize;
  object_class->get_property = gdk_subsurface_get_property;

  g_object_class_install_property (object_class,
                                   PROP_DMABUF_FORMATS,
                                   g_param_spec_boxed ("dmabuf-formats", NULL, NULL,
                                                       GDK_TYPE_DMABUF_FORMATS,
                                                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

GdkSurface *
gdk_subsurface_get_parent (GdkSubsurface *subsurface)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), NULL);

  return subsurface->parent;
}

gboolean
gdk_subsurface_attach (GdkSubsurface         *subsurface,
                       GdkTexture            *texture,
                       const graphene_rect_t *rect,
                       gboolean               above,
                       GdkSubsurface         *sibling)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), FALSE);
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);
  g_return_val_if_fail (sibling == NULL || GDK_IS_SUBSURFACE (sibling), FALSE);

  return GDK_SUBSURFACE_GET_CLASS (subsurface)->attach (subsurface, texture, rect, above, sibling);
}

void
gdk_subsurface_detach (GdkSubsurface *subsurface)
{
  g_return_if_fail (GDK_IS_SUBSURFACE (subsurface));

  GDK_SUBSURFACE_GET_CLASS (subsurface)->detach (subsurface);
}

GdkTexture *
gdk_subsurface_get_texture (GdkSubsurface *subsurface)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), NULL);

  return GDK_SUBSURFACE_GET_CLASS (subsurface)->get_texture (subsurface);
}

void
gdk_subsurface_get_rect (GdkSubsurface   *subsurface,
                         graphene_rect_t *rect)
{
  g_return_if_fail (GDK_IS_SUBSURFACE (subsurface));
  g_return_if_fail (rect != NULL);

  GDK_SUBSURFACE_GET_CLASS (subsurface)->get_rect (subsurface, rect);
}

gboolean
gdk_subsurface_is_above_parent (GdkSubsurface *subsurface)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), TRUE);

  return GDK_SUBSURFACE_GET_CLASS (subsurface)->is_above_parent (subsurface);
}

void
gdk_subsurface_set_dmabuf_formats (GdkSubsurface    *subsurface,
                                   GdkDmabufFormats *formats)
{
  g_return_if_fail (GDK_IS_SUBSURFACE (subsurface));
  g_return_if_fail (formats != NULL);

  g_clear_pointer (&subsurface->dmabuf_formats, gdk_dmabuf_formats_unref);
  subsurface->dmabuf_formats = gdk_dmabuf_formats_ref (formats);

  g_object_notify (G_OBJECT (subsurface), "dmabuf-formats");
}

GdkDmabufFormats *
gdk_subsurface_get_dmabuf_formats (GdkSubsurface *subsurface)
{
  g_return_val_if_fail (GDK_IS_SUBSURFACE (subsurface), NULL);

  return subsurface->dmabuf_formats;
}
