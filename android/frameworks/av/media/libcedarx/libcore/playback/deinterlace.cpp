/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : deinterlace.cpp
 * Description : deinterlace
 * History :
 *
 */

#include <deinterlace.h>
#include <deinterlaceHw.h>

#if (CONFIG_DEINTERLACE == OPTION_SW_DEINTERLACE)
#include <deinterlaceSw.h>
#endif

Deinterlace *DeinterlaceCreate()
{
    Deinterlace *di = NULL;

#if (CONFIG_DEINTERLACE == OPTION_HW_DEINTERLACE)
    di = new DeinterlaceHw();
#elif (CONFIG_DEINTERLACE == OPTION_SW_DEINTERLACE)
    /* PD should add sw interlace here... */
    di = new DeinterlaceSw();
#endif
    return di;
}
