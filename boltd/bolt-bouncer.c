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

#include "config.h"

#include "bolt-bouncer.h"

#include "bolt-str.h"

#include <gio/gio.h>
#include <polkit/polkit.h>

static void     bouncer_initable_iface_init (GInitableIface *iface);


static gboolean bouncer_initialize (GInitable    *initable,
                                    GCancellable *cancellable,
                                    GError      **error);

static gboolean handle_authorize_method (GDBusInterfaceSkeleton *interface,
                                         GDBusMethodInvocation  *invocation,
                                         gpointer                user_data);


#ifndef HAVE_POLKIT_AUTOPTR
G_DEFINE_AUTOPTR_CLEANUP_FUNC (PolkitAuthorizationResult, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (PolkitDetails, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (PolkitSubject, g_object_unref)
#endif

struct _BoltBouncer
{
  GObject object;

  /* */
  PolkitAuthority *authority;
};

G_DEFINE_TYPE_WITH_CODE (BoltBouncer, bolt_bouncer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                bouncer_initable_iface_init));


static void
bolt_bouncer_finalize (GObject *object)
{
  BoltBouncer *bouncer = BOLT_BOUNCER (object);

  g_clear_object (&bouncer->authority);

  G_OBJECT_CLASS (bolt_bouncer_parent_class)->finalize (object);
}

static void
bolt_bouncer_init (BoltBouncer *bouncer)
{
}

static void
bolt_bouncer_class_init (BoltBouncerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = bolt_bouncer_finalize;

}

static void
bouncer_initable_iface_init (GInitableIface *iface)
{
  iface->init = bouncer_initialize;
}

static gboolean
bouncer_initialize (GInitable    *initable,
                    GCancellable *cancellable,
                    GError      **error)
{
  BoltBouncer *bnc = BOLT_BOUNCER (initable);

  bnc->authority = polkit_authority_get_sync (cancellable, error);
  return bnc->authority != NULL;
}

/* internal methods */
static gboolean
handle_authorize_method (GDBusInterfaceSkeleton *iface,
                         GDBusMethodInvocation  *inv,
                         gpointer                user_data)
{
  g_autoptr(PolkitSubject) subject = NULL;
  g_autoptr(PolkitDetails) details = NULL;
  gboolean authorized = FALSE;
  BoltBouncer *bnc;
  const char *method_name;
  const char *sender;
  const char *action;

  bnc = BOLT_BOUNCER (user_data);
  method_name = g_dbus_method_invocation_get_method_name (inv);
  sender = g_dbus_method_invocation_get_sender (inv);

  subject = polkit_system_bus_name_new (sender);
  details = polkit_details_new ();

  action = NULL;

  if (bolt_streq (method_name, "EnrollDevice"))
    action = "org.freedesktop.bolt.enroll";
  else if (bolt_streq (method_name, "Authorize"))
    action = "org.freedesktop.bolt.authorize";
  else if (bolt_streq (method_name, "ForgetDevice"))
    action = "org.freedesktop.bolt.manage";
  else if (bolt_streq (method_name, "ListDevices"))
    authorized = TRUE;
  else if (bolt_streq (method_name, "DeviceByUid"))
    authorized = TRUE;

  if (!authorized && action)
    {
      PolkitCheckAuthorizationFlags flags;
      g_autoptr(PolkitAuthorizationResult) res = NULL;
      g_autoptr(GError) error = NULL;

      flags = POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION;
      res = polkit_authority_check_authorization_sync (bnc->authority,
                                                       subject,
                                                       action, details,
                                                       flags,
                                                       NULL, &error);
      if (res == NULL)
        {
          g_dbus_method_invocation_return_error (inv, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                                                 "Authorization error: %s",
                                                 error->message);
          return FALSE;
        }

      authorized = polkit_authorization_result_get_is_authorized (res);
    }

  if (authorized == FALSE)
    g_dbus_method_invocation_return_error (inv, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED,
                                           "Bolt operation '%s' not allowed for user",
                                           method_name);

  return authorized;
}

/* public methods */
BoltBouncer *
bolt_bouncer_new (GCancellable *cancellable,
                  GError      **error)
{
  return g_initable_new (BOLT_TYPE_BOUNCER,
                         cancellable, error,
                         NULL);
}

void
bolt_bouncer_add_client (BoltBouncer *bnc,
                         gpointer     client)
{
  g_return_if_fail (G_IS_DBUS_INTERFACE_SKELETON (client));

  g_signal_connect (client, "g-authorize-method",
                    G_CALLBACK (handle_authorize_method),
                    bnc);
}
