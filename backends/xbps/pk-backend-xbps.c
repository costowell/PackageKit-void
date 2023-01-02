/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Cole Stowell <cole@stowell.pro>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "pk-backend-job.h"
#include <gmodule.h>
#include <glib.h>
#include <pk-backend.h>
#include <xbps.h>
#include <xbps/xbps_dictionary.h>

typedef struct PkBackendXBPSPriv {
  struct xbps_handle handle;
} PkBackendXBPSPriv;

static PkBackendXBPSPriv *priv;

void
pk_backend_initialize (GKeyFile *conf, PkBackend *backend)
{
  int rv = 0;
  priv = g_new0(PkBackendXBPSPriv, 1);
  
  if ((rv = xbps_init(&priv->handle)) != 0) {
    g_error("Failed to initialize libxbps: %s", strerror(rv));
  }
}

int add_pkgs_from_repo(struct xbps_repo *repo, void *arg, bool *done)
{
  PkBackendJob *job = (PkBackendJob *)arg;
  xbps_dictionary_t pkgd;
  xbps_object_t obj;
  PkInfoEnum pk_state;
  xbps_object_iterator_t iter;
  const char *pkgver, *arch, *short_desc, *version, *repository;
  char *pkg_id = malloc(200 * sizeof(char));
  char *pkg_name = malloc(50 * sizeof(char));

  /* ignore empty repos */
  if (repo->idx == NULL)
    return 0;

  int length = xbps_dictionary_count(repo->idx);
  unsigned int count = 0;
  iter = xbps_dictionary_iterator(repo->idx);
  while ((obj = xbps_object_iterator_next(iter)) != NULL) {

    pkgd = xbps_dictionary_get_keysym(repo->idx, obj);
    xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
    xbps_dictionary_get_cstring_nocopy(pkgd, "repository", &repository);
    xbps_dictionary_get_cstring_nocopy(pkgd, "architecture", &arch);
    xbps_dictionary_get_cstring_nocopy(pkgd, "short_desc", &short_desc);
    xbps_pkg_name(pkg_name, 50, pkgver);
    version = xbps_pkg_version(pkgver);

    snprintf(pkg_id, 200, "%s;%s;%s;%s", pkg_name, version, arch, repository);

    if (xbps_pkgdb_get_pkg(repo->xhp, pkgver))
      pk_state = PK_INFO_ENUM_INSTALLED;
    else
      pk_state = PK_INFO_ENUM_AVAILABLE;

    pk_backend_job_package(job, pk_state, pkg_id, short_desc);

    pk_backend_job_set_percentage(job, (++count/length) * 100);
  }

  return 0;
}

void
pk_backend_get_packages(PkBackend *backend, PkBackendJob *job, PkBitfield filters) {
  pk_backend_job_set_status(job, PK_STATUS_ENUM_QUERY);
  pk_backend_job_set_allow_cancel(job, TRUE);
  xbps_rpool_foreach(&priv->handle, add_pkgs_from_repo, job);
  pk_backend_job_finished(job);
}
  
const gchar *
pk_backend_get_description (PkBackend *backend)
{
	return g_strdup("xbps");
}

const gchar *
pk_backend_get_author(PkBackend *backend)
{
  return g_strdup("Cole Stowell <cole@stowell.pro>");
}

char *
pk_xbps_arr_to_str(xbps_array_t *arr)
{
  const char *str = NULL;
  unsigned int maxlen = 128, len = 0;
  char *ret = malloc(maxlen * sizeof(char));
  
  for (unsigned int i = 0; i < xbps_array_count(*arr); i++) {
    xbps_array_get_cstring_nocopy(*arr, i, &str);
    if (len += (strlen(str) + 2) >= maxlen) {
      maxlen += 128;
      ret = realloc(ret, maxlen);
    }
    if (i != 0) strcat(ret, ", ");
    strcat(ret, str);
  }
  return ret;
}

void
pk_backend_get_updates(PkBackend *backend, PkBackendJob *job, PkBitfield filters)
{
  int rv = 0;
  xbps_array_t array;
  xbps_object_iterator_t iter;
  xbps_object_t obj;
  PkInfoEnum pk_state;
  char *arr_str = NULL;
  const char *pkgver, *arch, *short_desc, *version, *repository;
  char *pkg_id = malloc(200 * sizeof(char));
  char *pkg_name = malloc(50 * sizeof(char));
  
  if ((rv = xbps_transaction_update_packages(&priv->handle)) != 0) {
    if (rv == ENOENT) {
      pk_backend_job_error_code(job, PK_ERROR_ENUM_NO_PACKAGES_TO_UPDATE, "No packages currently registered");
      pk_backend_job_finished(job);
    } else if (rv == EBUSY) {
      rv = 0;
    } else if (rv == EEXIST) {
      rv = 0;
    } else if (rv == ENOTSUP) {
      pk_backend_job_error_code(job, PK_ERROR_ENUM_REPO_NOT_FOUND, "No repositories currently registered");
      pk_backend_job_finished(job);
    } else {
      pk_backend_job_error_code(job, PK_ERROR_ENUM_UNKNOWN, "Unexpected error %s", strerror(rv));
      pk_backend_job_finished(job);
    }
  }

  if ((rv = xbps_transaction_prepare(&priv->handle)) != 0) {
    if (rv == ENODEV) {
      array = xbps_dictionary_get(priv->handle.transd, "missing_deps");
      if (xbps_array_count(array)) {
        /* missing dependencies */
        arr_str = pk_xbps_arr_to_str(&array);
        pk_backend_job_error_code(job, PK_ERROR_ENUM_DEP_RESOLUTION_FAILED, "Transaction aborted due to unresolved dependencies: %s", arr_str);
      }
    } else if (rv == ENOEXEC) {
      array = xbps_dictionary_get(priv->handle.transd, "missing_shlibs");
      if (xbps_array_count(array)) {
        arr_str = pk_xbps_arr_to_str(&array);
        pk_backend_job_error_code(job, PK_ERROR_ENUM_DEP_RESOLUTION_FAILED, "Transaction aborted due to unresolved shlibs: %s", arr_str);
      }
    } else if (rv == EAGAIN) { 
      array = xbps_dictionary_get(priv->handle.transd, "conflicts");
      arr_str = pk_xbps_arr_to_str(&array);
      pk_backend_job_error_code(job, PK_ERROR_ENUM_PACKAGE_CONFLICTS, "Transaction aborted due to conflicting packages: %s", arr_str);
    } else if (rv == ENOSPC) {
      pk_backend_job_error_code(job, PK_ERROR_ENUM_NO_SPACE_ON_DEVICE, "Transaction aborted due to insufficient disk");
    }
    if (arr_str) free(arr_str);
    pk_backend_job_finished(job);
  }

  iter = xbps_array_iter_from_dict(priv->handle.transd, "packages");

  while ((obj = xbps_object_iterator_next(iter)) != NULL) {
    xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
    xbps_dictionary_get_cstring_nocopy(obj, "repository", &repository);
    xbps_dictionary_get_cstring_nocopy(obj, "architecture", &arch);
    xbps_dictionary_get_cstring_nocopy(obj, "short_desc", &short_desc);
    xbps_pkg_name(pkg_name, 50, pkgver);
    version = xbps_pkg_version(pkgver);

    snprintf(pkg_id, 200, "%s;%s;%s;%s", pkg_name, version, arch, repository);

    if (xbps_pkgdb_get_pkg(&priv->handle, pkgver))
      pk_state = PK_INFO_ENUM_INSTALLED;
    else
      pk_state = PK_INFO_ENUM_AVAILABLE;

    pk_backend_job_package(job, pk_state, pkg_id, short_desc);
  }

  pk_backend_job_finished(job);

}


PkBitfield
pk_backend_get_groups(PkBackend *backend)
{
  PkBitfield bitfield = 0;
  pk_bitfield_add(bitfield, PK_GROUP_ENUM_SYSTEM);
  return bitfield;
}
