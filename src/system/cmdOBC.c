/*                                 SUCHAI
 *                      NANOSATELLITE FLIGHT SOFTWARE
 *
 *      Copyright 2020, Carlos Gonzalez Cortes, carlgonz@uchile.cl
 *      Copyright 2020, Tomas Opazo Toro, tomas.opazo.t@gmail.com
 *      Copyright 2020, Matias Ramirez Martinez, nicoram.mt@gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "suchai/cmdOBC.h"

static const char* tag = "cmdOBC";

#define TLE_BUFF_LEN 70
TLE tle;
static char tle1[TLE_BUFF_LEN]; //"1 42788U 17036Z   20054.20928660  .00001463  00000-0  64143-4 0  9996";
static char tle2[TLE_BUFF_LEN]; //"2 42788  97.3188 111.6825 0013081  74.6084 285.6598 15.23469130148339";

void cmd_obc_init(void)
{
    cmd_add("obc_ident", obc_ident, "", 0);
    cmd_add("obc_debug", obc_debug, "%d", 1);
    cmd_add("obc_reset", obc_reset, "", 0);
    cmd_add("obc_get_mem", obc_get_os_memory, "", 0);
    cmd_add("obc_set_time", obc_set_time,"%d",1);
    cmd_add("obc_get_time", obc_get_time, "%d", 1);
    cmd_add("obc_reset_wdt", obc_reset_wdt, "", 0);
    cmd_add("obc_system", obc_system, "%s", 1);
}

int obc_ident(char* fmt, char* params, int nparams)
{
    printf("Name: %s\r\nID  : %d\r\nVer : %s\r\nNode: %d\r\n",
            SCH_NAME, SCH_DEVICE_ID, SCH_SW_VERSION, SCH_COMM_NODE);
    return CMD_OK;
}

int obc_debug(char *fmt, char *params, int nparams)
{
    int dbg_type;
    if(params == NULL || sscanf(params, fmt, &dbg_type) != nparams)
    {
        LOGE(tag, "Parameter null");
        return CMD_SYNTAX_ERROR;
    }

    #ifdef AVR32
        switch(dbg_type)
        {
            case 0: LED_Toggle(LED0); break;
            case 1: LED_Toggle(LED1); break;
            case 2: LED_Toggle(LED2); break;
            case 3: LED_Toggle(LED3); break;
            default: LED_Toggle(LED0);
        }
    #endif
    #ifdef NANOMIND
        if(dbg_type <= GS_A3200_LED_A)
            gs_a3200_led_toggle((gs_a3200_led_t)dbg_type);
    #endif
    #ifdef ESP32
        static int level = 0;
        level = ~level;
        gpio_set_level(BLINK_GPIO, level);
    #endif
    #ifdef LINUX
        LOGV(tag, "OBC Debug (%d)", dbg_type);
    #endif
    return CMD_OK;

}

int obc_reset_wdt(char *fmt, char *params, int narams)
{
    #ifdef NANOMIND
       wdt_clear();
    #endif
    #ifdef AVR32
       wdt_clear();
    #endif
    return CMD_OK;
}

int obc_reset(char *fmt, char *params, int nparams)
{
    printf("Resetting system NOW!!\r\n");

    #ifdef LINUX
        if(params != NULL && strcmp(params, "reboot")==0)
            system("sudo reboot");
        else
            exit(0);
    #endif
    #ifdef AVR32
        reset_do_soft_reset();
    #endif
    #ifdef NANOMIND
        cpu_reset();
    #endif

    /* Never get here */
    return CMD_OK;
}

int obc_get_os_memory(char *fmt, char *params, int nparams)
{
#ifdef SCH_HAVE_MALLOC
    struct mallinfo mi;
    mi = mallinfo();
    LOGR(tag, "Total non-mmapped bytes (arena):       %d", mi.arena);
    LOGR(tag, "# of free chunks (ordblks):            %d", mi.ordblks);
    LOGR(tag, "# of free fastbin blocks (smblks):     %d", mi.smblks);
    LOGR(tag, "# of mapped regions (hblks):           %d", mi.hblks);
    LOGR(tag, "Bytes in mapped regions (hblkhd):      %d", mi.hblkhd);
    LOGR(tag, "Max. total allocated space (usmblks):  %d", mi.usmblks);
    LOGR(tag, "Free bytes held in fastbins (fsmblks): %d", mi.fsmblks);
    LOGR(tag, "Total allocated space (uordblks):      %d", mi.uordblks);
    LOGR(tag, "Total free space (fordblks):           %d", mi.fordblks);
    LOGR(tag, "Topmost releasable block (keepcost):   %d", mi.keepcost);
    return CMD_OK;
#else
    LOGE(tag, "Not implemented!");
    return CMD_ERROR;
#endif
}

int obc_set_time(char* fmt, char* params,int nparams)
{
    int time_to_set;
    if(params == NULL || sscanf(params, fmt, &time_to_set) != nparams)
    {
        LOGE(tag, "Invalid params");
        return CMD_SYNTAX_ERROR;
    }

    int rc = dat_set_time(time_to_set);
    if (rc == 0)
        return CMD_OK;
    else
        return CMD_ERROR;
}

int obc_get_time(char *fmt, char *params, int nparams)
{
    int format = 0;
    if((params == NULL) || (sscanf(params, fmt, &format) < nparams))
    {
        format = 0;
    }

    int rc = dat_show_time(format);  // LOGR inside
    return (rc == 0) ? CMD_OK : CMD_ERROR;
}

int obc_system(char* fmt, char* params, int nparams)
{
#ifdef LINUX
    if(params != NULL)
    {
        int rc = system(params);
        if(rc < 0)
        {
            LOGR(tag, "Call to system failed! (%d)", rc)
            return CMD_ERROR;
        }
        else
        {
            LOGR(tag, "Call to system returned (%d)", rc);
            return CMD_OK;
        }
    }
    else
    {
        LOGE(tag, "Parameter null");
        return CMD_SYNTAX_ERROR;
    }
#else
    LOGW(tag, "Command not suported!");
    return CMD_ERROR;
#endif
}
