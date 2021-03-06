/*
 * Copyright © 2017 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Christian J. Kellner <christian@kellner.me>
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct BoltProxyProp
{

  const char *theirs;
  const char *ours;
  guint       prop_id;

  /* GVariant to GValue, if any */
  void (*convert)(GVariant *input,
                  GValue   *output);

} BoltProxyProp;


typedef struct BoltProxySignal
{

  const char *theirs;
  void (*handle)(GObject    *self,
                 GDBusProxy *bus_proxy,
                 GVariant   *params);

} BoltProxySignal;

#define BOLT_TYPE_PROXY (bolt_proxy_get_type ())
G_DECLARE_DERIVABLE_TYPE (BoltProxy, bolt_proxy, BOLT, PROXY, GObject)

typedef struct _BoltProxyPrivate BoltProxyPrivate;

struct _BoltProxyClass
{
  GObjectClass parent;

  /* virtuals */
  const BoltProxyProp   * (*get_dbus_props) (guint *n);
  const BoltProxySignal * (*get_dbus_signals) (guint *n);
};

gboolean          bolt_proxy_get_dbus_property (GObject *proxy,
                                                guint    prop_id,
                                                GValue  *value);

GDBusProxy *      bolt_proxy_get_proxy (BoltProxy *proxy);
const char *      bolt_proxy_get_object_path (BoltProxy *proxy);

G_END_DECLS
