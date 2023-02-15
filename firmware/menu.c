/*
 * Copyright (c) 2019-2022 Kim Jørgensen
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "menu.h"
#include "menu_sd.h"
#include "menu_d64.h"
#include "menu_t64.h"
#include "menu_options.h"
#include "d64.c"
#include "t64.c"
#include "loader.c"
#include "menu_sd.c"
#include "menu_d64.c"
#include "menu_t64.c"
#include "menu_options.c"
#include "menu_settings.c"

static void menu_loop()
{
    menu = sd_menu_init();
    char *search = sd_state.search;

    u8 cmd = CMD_MENU;
    bool should_save_dat = true;

    dbg("Waiting for commands...");
    while (true)
    {
        c64_set_command(cmd);
        u8 reply;
        while (!c64_get_reply(cmd, &reply))
        {
            if (usb_gotc())
            {
                dat_file.boot_type = DAT_USB;
                should_save_dat = false;
                cmd = CMD_WAIT_SYNC;
                break;
            }
        }

        if (cmd == CMD_WAIT_SYNC)
        {
            break;
        }

        u8 data;
        switch (reply)
        {
            case REPLY_OK:
                cmd = CMD_NONE;
                break;

            case REPLY_DIR:
                c64_receive_string(search);
                convert_to_ascii(search, (u8 *)search, SEARCH_LENGTH+1);
                cmd = menu->dir(menu->state);
                break;

            case REPLY_DIR_ROOT:
                cmd = menu->dir_up(menu->state, true);
                break;

            case REPLY_DIR_UP:
                cmd = menu->dir_up(menu->state, false);
                break;

            case REPLY_DIR_PREV_PAGE:
                cmd = menu->prev_page(menu->state);
                break;

            case REPLY_DIR_NEXT_PAGE:
                cmd = menu->next_page(menu->state);
                break;

            case REPLY_SELECT:
                data = c64_receive_byte();
                cmd = menu->select(menu->state, data & 0xc0, data & 0x3f);
                break;

            case REPLY_SETTINGS:
                cmd = handle_settings();
                break;

            case REPLY_BASIC:
                dat_file.boot_type = DAT_BASIC;
                should_save_dat = persist_basic_selection();
                cmd = CMD_WAIT_SYNC;
                break;

            case REPLY_KILL:
                dat_file.boot_type = DAT_KILL;
                should_save_dat = persist_basic_selection();
                cmd = CMD_WAIT_SYNC;
                break;

            case REPLY_KILL_C128:
                dat_file.boot_type = DAT_KILL_C128;
                should_save_dat = persist_basic_selection();
                cmd = CMD_WAIT_SYNC;
                break;

            case REPLY_RESET:
                c64_send_command(CMD_WAIT_RESET);
                system_restart();
                break;

            default:
                wrn("Got unknown reply: %x", reply);
                cmd = CMD_NONE;
                break;
        }

        if (!c64_interface_active())
        {
            break;
        }
    }

    c64_interface(false);
    if (should_save_dat)
    {
        save_dat();
    }
}

static void fail_to_read_sd(void)
{
    c64_send_warning("Failed to read SD card");
    restart_to_menu();
}

static OPTIONS_STATE * build_options(const char *title, const char *message)
{
    OPTIONS_STATE *options = options_init(title);
    options_add_empty(options);
    options_add_text_block(options, message);

    return options;
}

static u8 handle_unsupported_ex(const char *title, const char *message, const char *file_name)
{
    OPTIONS_STATE *options = build_options(title, message);

    options_add_text_block(options, file_name);
    options_add_dir(options, "OK");

    return handle_options();
}

static u8 handle_unsupported(const char *file_name)
{
    return handle_unsupported_ex("Unsupported",
                                 "File is not supported or invalid", file_name);
}

static void handle_sid(SID_HEADER* header, const char *file_name)
{
	char tabs[7] = "   ";
	char songNameString[50];
	sprintf(songNameString, "%s%s", tabs, header->songName);

	char authorString[50];
	sprintf(authorString, "%s%s" ,tabs, header->author);

	char releasedString[50];
	sprintf(releasedString, "%s%s",tabs, header->released);

	char loadAddressString[25];
	sprintf(loadAddressString, "%sLoad Address: 0x%.4x",tabs, header->loadAddress);

	char initAddressString[25];
	sprintf(initAddressString, "%sInit Address: 0x%.4x",tabs, header->initAddress);

	char playAddressString[25];
	sprintf(playAddressString, "%sPlay Address: 0x%.4x",tabs, header->playAddress);

	char dataOffsetAddress[25];
	sprintf(dataOffsetAddress, "%sData offset:  0x%.4x",tabs, header->dataOffset);

	char magicIdString[25];
	sprintf(magicIdString, "%sMagic ID: %.*s",tabs, header->magicID);

	char numberOfSongsString[25];
	sprintf(numberOfSongsString, "%sSong count: %d",tabs, header->songs);

	char startSongString[25];
	sprintf(startSongString, "%sStart song: %d",tabs, header->startSong);

	char topline[255+20];
	sprintf(topline, "FILENAME: %s", file_name);

	 OPTIONS_STATE *options = build_options("SID information",topline);
	 options_add_text(options, "ABOUT SONG");
	 options_add_text(options, songNameString);
	 options_add_text(options, authorString);
	 options_add_text_block(options, releasedString);
	 options_add_text(options, "ADDRESSES");
	 options_add_text(options, loadAddressString);
	 options_add_text(options, initAddressString);
	 options_add_text(options, playAddressString);
	 options_add_text(options, dataOffsetAddress);
	 options_add_text_block(options, magicIdString);
	 options_add_text(options, "SONG PARTS");
	 options_add_text(options, numberOfSongsString);
	 options_add_text_block(options, startSongString);
	 options_add_dir(options, "OK");
	 handle_options(options);

	//handle_unsupported_ex("SID Info", "test", header->released);
	//options_add_text_block(options, header->songName);
	//options_add_dir(options, "OK");
}

static u8 handle_unsupported_warning(const char *message, const char *file_name,
                                     u8 element_no)
{
    OPTIONS_STATE *options = build_options("Unsupported", message);

    options_add_text_block(options, file_name);
    options_add_dir(options, "Cancel");
    options_add_select(options, "Continue", SELECT_FLAG_ACCEPT, element_no);

    return handle_options();
}

static u8 handle_unsaved_crt(const char *file_name, void (*handle_save)(u8))
{
    OPTIONS_STATE *options = build_options("Unsaved changes",
                                           "How do you want to save the changes to CRT?");
    options_add_text_block(options, file_name);
    options_add_callback(options, handle_save, "Overwrite file", SELECT_FLAG_OVERWRITE);
    options_add_callback(options, handle_save, "New file", 0);
    options_add_dir(options, "Cancel");

    return handle_options();
}

static u8 handle_file_options(const char *file_name, u8 file_type, u8 element_no)
{
    const char *title = "File Options";
    const char *select_text;
    const char *vic_text = NULL;
    const char *mount_text = NULL;
    bool delete_option = true;

    switch (file_type)
    {
        case FILE_DIR:
            mount_text = "Mount";
        case FILE_DIR_UP:
            title = "Directory Options";
            select_text = "Open";
            delete_option = false;
            break;

        case FILE_CRT:
            select_text = "Run";
            vic_text = "Run (VIC-II/C128 mode)";
            break;

        case FILE_P00:
            select_text = "Load";
            break;

        case FILE_D64:
            select_text = "Open";
            mount_text = "Mount";
            break;

        case FILE_D64_STAR:
            delete_option = false;
        case FILE_D64_PRG:
        case FILE_PRG:
            select_text = "Mount and load";
            mount_text = "Load"; // No mount
            break;
        case FILE_SID:
			select_text = "Load";
			//vic_text = "Execute SID";
			delete_option = true;
			break;

        case FILE_T64_PRG:
            delete_option = false;
        case FILE_T64:
            select_text = "Open";
            break;

        default:
            select_text = "Select";
            break;
    }

    OPTIONS_STATE *options = build_options(title, file_name);
    options_add_select(options, select_text, SELECT_FLAG_ACCEPT, element_no);
    if (vic_text)
    {
        options_add_select(options, vic_text, SELECT_FLAG_VIC, element_no);
    }
    if (mount_text)
    {
        options_add_select(options, mount_text, SELECT_FLAG_MOUNT, element_no);
    }
    if (delete_option)
    {
        options_add_select(options, "Delete", SELECT_FLAG_DELETE, element_no);
    }
    options_add_dir(options, "Cancel");

    return handle_options();
}

static u8 handle_upgrade_menu(const char *firmware, u8 element_no)
{
    OPTIONS_STATE *options = build_options("Firmware Upgrade",
                                           "This will upgrade the firmware to");
    options_add_text_block(options, firmware);
    options_add_select(options, "Upgrade", SELECT_FLAG_ACCEPT, element_no);
    options_add_dir(options, "Cancel");
    return handle_options();
}

static const char * to_petscii_pad(char *dest, const char *src, u8 size)
{
    for (u8 i=0; i<size; i++)
    {
        char c = *src;
        if (c)
        {
            src++;
            *dest++ = ascii_to_petscii(c);
        }
        else
        {
            *dest++ = ' ';  // Pad with space
        }
    }

    return src;
}

static bool format_path(char *buf, bool include_file)
{
    bool in_root = false;
    *buf++ = ' ';

    u16 len;
    for (len = 0; len < sizeof(dat_file.path) && dat_file.path[len]; len++)
    {
        buf[len] = dat_file.path[len];
    }

    if (include_file)
    {
        if (len > 1)
        {
            buf[len++] = '/';
        }

        char *filename = dat_file.file;
        for (; *filename; len++)
        {
            buf[len] = *filename++;
        }
    }
    buf[len] = 0;

    u16 index = 0;
    if (len == 0)
    {
        buf[0] = '.';
        buf[1] = 0;
    }
    else if (len > DIR_NAME_LENGTH-2)
    {
        index = len - (DIR_NAME_LENGTH-2);
        buf[index] = '.';
        buf[index+1] = '.';
    }
    else if (len == 1 && buf[0] == '/')
    {
        in_root = true;
    }

    to_petscii_pad(buf, buf + index, DIR_NAME_LENGTH-1);
    return in_root;
}

static void send_page_end(void)
{
    scratch_buf[0] = 0;
    c64_send_data(scratch_buf, ELEMENT_LENGTH);
}

static u8 handle_page_end(void)
{
    send_page_end();
    return CMD_READ_DIR_PAGE;
}
