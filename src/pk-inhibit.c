/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#include <glib/gi18n.h>
#include <glib.h>
#include <dbus/dbus-glib.h>

#include "pk-debug.h"
#include "pk-inhibit.h"

#define PK_INHIBIT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PK_TYPE_INHIBIT, PkInhibitPrivate))
#define HAL_DBUS_SERVICE		"org.freedesktop.Hal"
#define HAL_DBUS_PATH_COMPUTER		"/org/freedesktop/Hal/devices/computer"
#define HAL_DBUS_INTERFACE_DEVICE	"org.freedesktop.Hal.Device"
#define HAL_DBUS_INTERFACE_PM		"org.freedesktop.Hal.Device.SystemPowerManagement"


struct PkInhibitPrivate
{
	GPtrArray		*array;
	gboolean		 is_locked;
	DBusGProxy		*proxy;
};

enum {
	PK_INHIBIT_LOCKED,
	PK_INHIBIT_LAST_SIGNAL
};

static gpointer pk_inhibit_object = NULL;
static guint signals [PK_INHIBIT_LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (PkInhibit, pk_inhibit, G_TYPE_OBJECT)

/**
 * pk_inhibit_locked:
 **/
gboolean
pk_inhibit_locked (PkInhibit *inhibit)
{
	g_return_val_if_fail (inhibit != NULL, FALSE);
	g_return_val_if_fail (PK_IS_INHIBIT (inhibit), FALSE);
	return inhibit->priv->is_locked;
}

/**
 * pk_inhibit_lock:
 **/
G_GNUC_WARN_UNUSED_RESULT static gboolean
pk_inhibit_lock (PkInhibit *inhibit)
{
	GError *error = NULL;
	gboolean ret;

	g_return_val_if_fail (inhibit != NULL, FALSE);
	g_return_val_if_fail (PK_IS_INHIBIT (inhibit), FALSE);

	pk_debug ("Trying to AcquireInterfaceLock");
	if (inhibit->priv->proxy == NULL) {
		pk_warning ("not connected to HAL");
		return FALSE;
	}
	if (inhibit->priv->is_locked == TRUE) {
		pk_warning ("already inhibited, not trying again");
		return FALSE;
	}

	/* Lock the interface */
	ret = dbus_g_proxy_call (inhibit->priv->proxy, "AcquireInterfaceLock", &error,
				 G_TYPE_STRING, HAL_DBUS_INTERFACE_PM,
				 G_TYPE_BOOLEAN, FALSE,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error != NULL) {
		printf ("DEBUG: ERROR: %s\n", error->message);
		g_error_free (error);
	}
	if (ret == TRUE) {
		inhibit->priv->is_locked = TRUE;
		pk_debug ("emit lock %i", inhibit->priv->is_locked);
		g_signal_emit (inhibit, signals [PK_INHIBIT_LOCKED], 0, inhibit->priv->is_locked);
	}

	return ret;
}

/**
 * pk_inhibit_unlock:
 **/
G_GNUC_WARN_UNUSED_RESULT static gboolean
pk_inhibit_unlock (PkInhibit *inhibit)
{
	GError *error = NULL;
	gboolean ret;

	g_return_val_if_fail (inhibit != NULL, FALSE);
	g_return_val_if_fail (PK_IS_INHIBIT (inhibit), FALSE);

	pk_debug ("Trying to ReleaseInterfaceLock");
	if (inhibit->priv->proxy == NULL) {
		pk_warning ("not connected to HAL");
		return FALSE;
	}
	if (inhibit->priv->is_locked == FALSE) {
		pk_warning ("not inhibited, not trying to unlock");
		return FALSE;
	}

	/* Lock the interface */
	ret = dbus_g_proxy_call (inhibit->priv->proxy, "ReleaseInterfaceLock", &error,
				 G_TYPE_STRING, HAL_DBUS_INTERFACE_PM,
				 G_TYPE_BOOLEAN, FALSE,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (error != NULL) {
		printf ("DEBUG: ERROR: %s\n", error->message);
		g_error_free (error);
	}
	if (ret == TRUE) {
		inhibit->priv->is_locked = FALSE;
		pk_debug ("emit lock %i", inhibit->priv->is_locked);
		g_signal_emit (inhibit, signals [PK_INHIBIT_LOCKED], 0, inhibit->priv->is_locked);
	}

	return ret;
}

/**
 * pk_inhibit_add:
 **/
gboolean
pk_inhibit_add (PkInhibit *inhibit, gpointer data)
{
	guint i;
	gboolean ret = TRUE;

	g_return_val_if_fail (inhibit != NULL, FALSE);
	g_return_val_if_fail (PK_IS_INHIBIT (inhibit), FALSE);

	for (i=0; i<inhibit->priv->array->len; i++) {
		if (g_ptr_array_index (inhibit->priv->array, i) == data) {
			pk_debug ("trying to add item %p already in array", data);
			return FALSE;
		}
	}
	g_ptr_array_add (inhibit->priv->array, data);
	/* do inhibit */
	if (inhibit->priv->array->len == 1) {
		ret = pk_inhibit_lock (inhibit);
	}
	return ret;
}

/**
 * pk_inhibit_remove:
 **/
gboolean
pk_inhibit_remove (PkInhibit *inhibit, gpointer data)
{
	guint i;
	gboolean ret = TRUE;

	g_return_val_if_fail (inhibit != NULL, FALSE);
	g_return_val_if_fail (PK_IS_INHIBIT (inhibit), FALSE);

	for (i=0; i<inhibit->priv->array->len; i++) {
		if (g_ptr_array_index (inhibit->priv->array, i) == data) {
			g_ptr_array_remove_index (inhibit->priv->array, i);
			if (inhibit->priv->array->len == 0) {
				ret = pk_inhibit_unlock (inhibit);
			}
			return ret;
		}
	}
	pk_debug ("cannot find item %p", data);
	return FALSE;
}

/**
 * pk_inhibit_finalize:
 **/
static void
pk_inhibit_finalize (GObject *object)
{
	PkInhibit *inhibit;
	gboolean ret;

	g_return_if_fail (object != NULL);
	g_return_if_fail (PK_IS_INHIBIT (object));
	inhibit = PK_INHIBIT (object);

	/* force an unlock if we are inhibited */
	if (inhibit->priv->is_locked == TRUE) {
		ret = pk_inhibit_unlock (inhibit);
		if (!ret) {
			pk_warning ("failed to unock on finalise!");
		}
	}
	g_ptr_array_free (inhibit->priv->array, TRUE);
	g_object_unref (inhibit->priv->proxy);

	G_OBJECT_CLASS (pk_inhibit_parent_class)->finalize (object);
}

/**
 * pk_inhibit_class_init:
 **/
static void
pk_inhibit_class_init (PkInhibitClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = pk_inhibit_finalize;
	signals [PK_INHIBIT_LOCKED] =
		g_signal_new ("locked", G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      0, NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
	g_type_class_add_private (klass, sizeof (PkInhibitPrivate));
}

/**
 * pk_inhibit_init:
 *
 * initializes the inhibit class. NOTE: We expect inhibit objects
 * to *NOT* be removed or added during the session.
 * We only control the first inhibit object if there are more than one.
 **/
static void
pk_inhibit_init (PkInhibit *inhibit)
{
	GError *error = NULL;
	DBusGConnection *connection;

	inhibit->priv = PK_INHIBIT_GET_PRIVATE (inhibit);
	inhibit->priv->is_locked = FALSE;
	inhibit->priv->array = g_ptr_array_new ();

	/* connect to system bus */
	connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error != NULL) {
		pk_warning ("Cannot connect to system bus: %s", error->message);
		g_error_free (error);
		return;
	}

	/* use gnome-power-manager for the battery detection */
	inhibit->priv->proxy = dbus_g_proxy_new_for_name_owner (connection,
				  HAL_DBUS_SERVICE, HAL_DBUS_PATH_COMPUTER,
				  HAL_DBUS_INTERFACE_DEVICE, &error);
	if (error != NULL) {
		pk_warning ("Cannot connect to HAL: %s", error->message);
		g_error_free (error);
	}

}

/**
 * pk_inhibit_new:
 * Return value: A new inhibit class instance.
 **/
PkInhibit *
pk_inhibit_new (void)
{
	if (pk_inhibit_object != NULL) {
		g_object_ref (pk_inhibit_object);
	} else {
		pk_inhibit_object = g_object_new (PK_TYPE_INHIBIT, NULL);
		g_object_add_weak_pointer (pk_inhibit_object, &pk_inhibit_object);
	}
	return PK_INHIBIT (pk_inhibit_object);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef PK_BUILD_TESTS
#include <libselftest.h>

void
libst_inhibit (LibSelfTest *test)
{
	PkInhibit *inhibit;
	gboolean ret;

	if (libst_start (test, "PkInhibit", CLASS_AUTO) == FALSE) {
		return;
	}

	/************************************************************/
	libst_title (test, "get an instance");
	inhibit = pk_inhibit_new ();
	if (inhibit != NULL) {
		libst_success (test, NULL);
	} else {
		libst_failed (test, NULL);
	}

	/************************************************************/
	libst_title (test, "check we have a connection");
	if (inhibit->priv->proxy != NULL) {
		libst_success (test, "got proxy");
	} else {
		libst_failed (test, "could not get proxy");
	}

	/************************************************************/
	libst_title (test, "check we are not inhibited");
	ret = pk_inhibit_locked (inhibit);
	if (ret == FALSE) {
		libst_success (test, "marked correctly");
	} else {
		libst_failed (test, "not marked correctly");
	}

	/************************************************************/
	libst_title (test, "add 123");
	ret = pk_inhibit_add (inhibit, GUINT_TO_POINTER (123));
	if (ret == TRUE) {
		libst_success (test, "inhibited");
	} else {
		libst_failed (test, "could not inhibit");
	}

	/************************************************************/
	libst_title (test, "check we are inhibited");
	ret = pk_inhibit_locked (inhibit);
	if (ret == TRUE) {
		libst_success (test, "marked correctly");
	} else {
		libst_failed (test, "not marked correctly");
	}

	/************************************************************/
	libst_title (test, "add 123 (again)");
	ret = pk_inhibit_add (inhibit, GUINT_TO_POINTER (123));
	if (ret == FALSE) {
		libst_success (test, "correctly ignored second");
	} else {
		libst_failed (test, "added the same number twice");
	}

	/************************************************************/
	libst_title (test, "add 456");
	ret = pk_inhibit_add (inhibit, GUINT_TO_POINTER (456));
	if (ret == TRUE) {
		libst_success (test, "inhibited");
	} else {
		libst_failed (test, "could not inhibit");
	}

	/************************************************************/
	libst_title (test, "remove 123");
	ret = pk_inhibit_remove (inhibit, GUINT_TO_POINTER (123));
	if (ret == TRUE) {
		libst_success (test, "removed first inhibit");
	} else {
		libst_failed (test, "could not remove inhibit");
	}

	/************************************************************/
	libst_title (test, "check we are still inhibited");
	ret = pk_inhibit_locked (inhibit);
	if (ret == TRUE) {
		libst_success (test, "marked correctly");
	} else {
		libst_failed (test, "not marked correctly");
	}

	/************************************************************/
	libst_title (test, "remove 456");
	ret = pk_inhibit_remove (inhibit, GUINT_TO_POINTER (456));
	if (ret == TRUE) {
		libst_success (test, "removed second inhibit");
	} else {
		libst_failed (test, "could not remove inhibit");
	}

	/************************************************************/
	libst_title (test, "check we are not inhibited");
	ret = pk_inhibit_locked (inhibit);
	if (ret == FALSE) {
		libst_success (test, "marked correctly");
	} else {
		libst_failed (test, "not marked correctly");
	}

	g_object_unref (inhibit);

	libst_end (test);
}
#endif

