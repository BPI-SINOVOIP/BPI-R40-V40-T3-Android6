/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : awPlayReadyLicense.h
 *
 * Description : deal with playReady license
 * History :
 *
 */

#ifndef AW_PLAYREADY_LICENSE_H
#define AW_PLAYREADY_LICENSE_H

#include <utils/Errors.h>
#include <binder/IInterface.h>

using namespace android;

status_t PlayReady_Drm_Invoke(const Parcel &request, Parcel *reply);

#endif /* AW_PLAYREADY_LICENSE_H */
