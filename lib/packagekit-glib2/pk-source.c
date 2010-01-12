/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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

/**
 * SECTION:pk-source
 * @short_description: Source object
 *
 * This GObject holds details about the source of the transaction object, and
 * are therefore shared properties that all data objects have.
 */

#include "config.h"

#include <glib-object.h>

#include <packagekit-glib2/pk-source.h>
#include <packagekit-glib2/pk-enum.h>

#include "egg-debug.h"

static void     pk_source_finalize	(GObject     *object);

#define PK_SOURCE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), PK_TYPE_SOURCE, PkSourcePrivate))

/**
 * PkSourcePrivate:
 *
 * Private #PkSource data
 **/
struct _PkSourcePrivate
{
	PkRoleEnum			 role;
	gchar				*transaction_id;
};

enum {
	PROP_0,
	PROP_ROLE,
	PROP_TRANSACTION_ID,
	PROP_LAST
};

G_DEFINE_TYPE (PkSource, pk_source, G_TYPE_OBJECT)

/**
 * pk_source_get_property:
 **/
static void
pk_source_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	PkSource *source = PK_SOURCE (object);
	PkSourcePrivate *priv = source->priv;

	switch (prop_id) {
	case PROP_ROLE:
		g_value_set_uint (value, priv->role);
		break;
	case PROP_TRANSACTION_ID:
		g_value_set_string (value, priv->transaction_id);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * pk_source_set_property:
 **/
static void
pk_source_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	PkSource *source = PK_SOURCE (object);
	PkSourcePrivate *priv = source->priv;

	switch (prop_id) {
	case PROP_ROLE:
		priv->role = g_value_get_uint (value);
		break;
	case PROP_TRANSACTION_ID:
		g_free (priv->transaction_id);
		priv->transaction_id = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * pk_source_class_init:
 **/
static void
pk_source_class_init (PkSourceClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = pk_source_finalize;
	object_class->get_property = pk_source_get_property;
	object_class->set_property = pk_source_set_property;

	/**
	 * PkSource:role:
	 */
	pspec = g_param_spec_uint ("role", NULL, NULL,
				   0, G_MAXUINT, PK_ROLE_ENUM_UNKNOWN,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ROLE, pspec);

	/**
	 * PkSource:transaction-id:
	 */
	pspec = g_param_spec_string ("transaction-id", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_TRANSACTION_ID, pspec);

	g_type_class_add_private (klass, sizeof (PkSourcePrivate));
}

/**
 * pk_source_init:
 **/
static void
pk_source_init (PkSource *source)
{
	source->priv = PK_SOURCE_GET_PRIVATE (source);
}

/**
 * pk_source_finalize:
 **/
static void
pk_source_finalize (GObject *object)
{
	PkSource *source = PK_SOURCE (object);
	PkSourcePrivate *priv = source->priv;

	g_free (priv->transaction_id);

	G_OBJECT_CLASS (pk_source_parent_class)->finalize (object);
}

/**
 * pk_source_new:
 *
 * Return value: a new PkSource object.
 **/
PkSource *
pk_source_new (void)
{
	PkSource *source;
	source = g_object_new (PK_TYPE_SOURCE, NULL);
	return PK_SOURCE (source);
}
