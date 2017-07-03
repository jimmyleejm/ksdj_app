/****************************************************************************
 *  Copyright (c) 2013 Nuvoton Technology Corp. All rights reserved.
 *
 ****************************************************************************
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 ****************************************************************************/

#ifndef _DEFINED_H
#define _DEFINED_H

#ifndef PACKAGE_SOUND_DIR
#define PACKAGE_SOUND_DIR "."
#else
#undef	PACKAGE_SOUND_DIR
#define PACKAGE_SOUND_DIR	"/mnt/videodoor/share/sounds/linphone"
#endif

#ifndef PACKAGE_DATA_DIR
#define PACKAGE_DATA_DIR "."
#else
#undef PACKAGE_DATA_DIR
#define PACKAGE_DATA_DIR	"/mnt/videodoor/share"
#endif

#ifndef LINPHONE_PLUGINS_DIR
#define LINPHONE_PLUGINS_DIR "."
#else
#undef LINPHONE_PLUGINS_DIR
#define LINPHONE_PLUGINS_DIR	"/lib/mediastreamer/plugins"
#endif

#endif /* _DEFINED_H */
