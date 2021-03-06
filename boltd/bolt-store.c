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

#include "bolt-store.h"

#include "bolt-error.h"
#include "bolt-fs.h"
#include "bolt-io.h"

#include <string.h>

/* ************************************  */
/* BoltStore */

struct _BoltStore
{
  GObject object;

  GFile  *root;
  GFile  *devices;
  GFile  *keys;
};


enum {
  PROP_STORE_0,

  PROP_ROOT,

  PROP_STORE_LAST
};

static GParamSpec *store_props[PROP_STORE_LAST] = { NULL, };


enum {
  SIGNAL_DEVICE_ADDED,
  SIGNAL_DEVICE_REMOVED,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = {0};


G_DEFINE_TYPE (BoltStore,
               bolt_store,
               G_TYPE_OBJECT)


static void
bolt_store_finalize (GObject *object)
{
  BoltStore *store = BOLT_STORE (object);

  if (store->root)
    g_clear_object (&store->root);

  if (store->devices)
    g_clear_object (&store->devices);

  if (store->keys)
    g_clear_object (&store->keys);

  G_OBJECT_CLASS (bolt_store_parent_class)->finalize (object);
}

static void
bolt_store_init (BoltStore *store)
{
}

static void
bolt_store_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  BoltStore *store = BOLT_STORE (object);

  switch (prop_id)
    {
    case PROP_ROOT:
      g_value_set_object (value, store->root);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bolt_store_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  BoltStore *store = BOLT_STORE (object);

  switch (prop_id)
    {
    case PROP_ROOT:
      store->root = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bolt_store_constructed (GObject *obj)
{
  BoltStore *store = BOLT_STORE (obj);

  store->devices = g_file_get_child (store->root, "devices");
  store->keys = g_file_get_child (store->root, "keys");
}

static void
bolt_store_class_init (BoltStoreClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = bolt_store_finalize;

  gobject_class->constructed  = bolt_store_constructed;
  gobject_class->get_property = bolt_store_get_property;
  gobject_class->set_property = bolt_store_set_property;

  store_props[PROP_ROOT] =
    g_param_spec_object ("root",
                         NULL, NULL,
                         G_TYPE_FILE,
                         G_PARAM_READWRITE      |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_NAME);

  g_object_class_install_properties (gobject_class,
                                     PROP_STORE_LAST,
                                     store_props);

  signals[SIGNAL_DEVICE_ADDED] =
    g_signal_new ("device-added",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);

  signals[SIGNAL_DEVICE_REMOVED] =
    g_signal_new ("device-removed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);
}

/* internal methods */

#define DEVICE_GROUP "device"
#define USER_GROUP "user"


/* public methods */

BoltStore *
bolt_store_new (const char *path)
{
  g_autoptr(GFile) root = NULL;
  BoltStore *store;

  root = g_file_new_for_path (path);
  store = g_object_new (BOLT_TYPE_STORE,
                        "root", root,
                        NULL);

  return store;
}

GStrv
bolt_store_list_uids (BoltStore *store,
                      GError   **error)
{
  g_autoptr(GDir) dir   = NULL;
  g_autofree char *path = NULL;
  GPtrArray *ids;
  const char *name;

  path = g_file_get_path (store->devices);

  dir = g_dir_open (path, 0, error);
  if (dir == NULL)
    return NULL;

  ids = g_ptr_array_new ();

  while ((name = g_dir_read_name (dir)) != NULL)
    {
      if (g_str_has_prefix (name, "."))
        continue;

      g_ptr_array_add (ids, g_strdup (name));
    }

  g_ptr_array_add (ids, NULL);
  return (GStrv) g_ptr_array_free (ids, FALSE);
}

gboolean
bolt_store_put_device (BoltStore  *store,
                       BoltDevice *device,
                       BoltPolicy  policy,
                       BoltKey    *key,
                       GError    **error)
{
  g_autoptr(GFile) entry = NULL;
  g_autoptr(GKeyFile) kf = NULL;
  g_autofree char *data  = NULL;
  const char *uid;
  gboolean ok;
  gsize len;
  guint keystate = 0;

  g_return_val_if_fail (store != NULL, FALSE);
  g_return_val_if_fail (device != NULL, FALSE);

  uid = bolt_device_get_uid (device);
  g_assert (uid);

  entry = g_file_get_child (store->devices, uid);

  ok = bolt_fs_make_parent_dirs (entry, error);
  if (!ok)
    return FALSE;

  kf = g_key_file_new ();

  g_key_file_set_string (kf, DEVICE_GROUP, "name", bolt_device_get_name (device));
  g_key_file_set_string (kf, DEVICE_GROUP, "vendor", bolt_device_get_vendor (device));

  if (policy != BOLT_POLICY_INVALID)
    {
      const char *str = bolt_policy_to_string (policy);
      g_key_file_set_string (kf, USER_GROUP, "policy", str);
    }

  data = g_key_file_to_data (kf, &len, error);

  if (!data)
    return FALSE;

  if (key)
    {
      g_autoptr(GError) err  = NULL;
      g_autoptr(GFile) keypath = g_file_get_child (store->keys, uid);
      ok = bolt_fs_make_parent_dirs (keypath, &err);

      if (ok)
        ok = bolt_key_save_file (key, keypath, &err);

      if (!ok)
        g_warning ("failed to store key: %s", err->message);
      else
        keystate = bolt_key_get_state (key);
    }

  ok = g_file_replace_contents (entry,
                                data, len,
                                NULL, FALSE,
                                0,
                                NULL,
                                NULL, error);

  if (ok)
    {
      g_object_set (device,
                    "store", store,
                    "policy", policy,
                    "key", keystate,
                    NULL);

      g_signal_emit (store, signals[SIGNAL_DEVICE_ADDED], 0, uid);
    }

  return ok;
}

BoltDevice *
bolt_store_get_device (BoltStore *store, const char *uid, GError **error)
{
  g_autoptr(GKeyFile) kf = NULL;
  g_autoptr(GFile) db = NULL;
  g_autofree char *name = NULL;
  g_autofree char *vendor = NULL;
  g_autofree char *data  = NULL;
  g_autofree char *polstr = NULL;
  BoltPolicy policy;
  BoltKeyState key;
  gboolean ok;
  gsize len;

  g_return_val_if_fail (store != NULL, FALSE);
  g_return_val_if_fail (uid != NULL, FALSE);

  db = g_file_get_child (store->devices, uid);
  ok = g_file_load_contents (db, NULL,
                             &data, &len,
                             NULL,
                             error);

  if (!ok)
    return NULL;

  kf = g_key_file_new ();
  ok = g_key_file_load_from_data (kf, data, len, G_KEY_FILE_NONE, error);

  if (!ok)
    return NULL;

  name = g_key_file_get_string (kf, DEVICE_GROUP, "name", NULL);
  vendor = g_key_file_get_string (kf, DEVICE_GROUP, "vendor", NULL);
  polstr = g_key_file_get_string (kf, USER_GROUP, "policy", NULL);
  policy = bolt_policy_from_string (polstr);

  if (!bolt_policy_validate (policy))
    {
      g_warning ("[%s] invalid policy in store: %s", uid, polstr);
      policy = BOLT_POLICY_MANUAL;
    }

  key = bolt_store_have_key (store, uid);

  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (vendor != NULL, NULL);

  return g_object_new (BOLT_TYPE_DEVICE,
                       "uid", uid,
                       "name", name,
                       "vendor", vendor,
                       "status", BOLT_STATUS_DISCONNECTED,
                       "store", store,
                       "policy", policy,
                       "key", key,
                       NULL);
}

gboolean
bolt_store_del_device (BoltStore  *store,
                       const char *uid,
                       GError    **error)
{
  g_autoptr(GFile) devpath = NULL;
  gboolean ok;

  devpath = g_file_get_child (store->devices, uid);
  ok = g_file_delete (devpath, NULL, error);

  if (ok)
    g_signal_emit (store, signals[SIGNAL_DEVICE_REMOVED], 0, uid);

  return ok;
}

BoltKeyState
bolt_store_have_key (BoltStore  *store,
                     const char *uid)
{
  g_autoptr(GFileInfo) keyinfo = NULL;
  g_autoptr(GFile) keypath = NULL;
  g_autoptr(GError) err = NULL;
  guint key = BOLT_KEY_MISSING;

  keypath = g_file_get_child (store->keys, uid);
  keyinfo = g_file_query_info (keypath, "standard::*", 0, NULL, &err);

  if (keyinfo != NULL)
    key = BOLT_KEY_HAVE; /* todo: check size */
  else if (!bolt_err_notfound (err))
    g_warning ("error querying key info for %s: %s", uid, err->message);

  return key;
}

BoltKey *
bolt_store_get_key (BoltStore  *store,
                    const char *uid,
                    GError    **error)
{
  g_autoptr(GFile) keypath = NULL;

  keypath = g_file_get_child (store->keys, uid);

  return bolt_key_load_file (keypath, error);
}

gboolean
bolt_store_del_key (BoltStore  *store,
                    const char *uid,
                    GError    **error)
{
  g_autoptr(GFile) keypath = NULL;
  gboolean ok;

  keypath = g_file_get_child (store->keys, uid);
  ok = g_file_delete (keypath, NULL, error);

  return ok;
}
