/*                                 SUCHAI
 *                      NANOSATELLITE FLIGHT SOFTWARE
 *
 *      Copyright 2021, Carlos Gonzalez Cortes, carlgonz@uchile.cl
 *      Copyright 2021, Camilo Rojas Milla, camrojas@uchile.cl
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

#include "suchai/storage.h"
#include <libpq-fe.h>
#include <libpq.h>
#include <math.h>

#define ST_SQL_MAX_LEN  (1000)

///< Status variables buffer
static size_t status_entries = 0;

///< Flight plan buffer
static char* fp_table = NULL;

///< Payloads storage buffer
static int payloads_entries = 0;
static data_map_t *payloads_schema = NULL;
static char *payloads_table = NULL;

static int storage_is_open = 0;

static PGconn *conn = NULL;


int storage_init(const char *db_name)
{
    char *hostaddr = SCH_STORAGE_PGHOST;
    int port;
    char *dbname;
    char *user;
    char *password;

    /* if postgresql eventualy upgrades and
     * changes it params names, you can
     * change here without touching
     * everywhere else
     * */
    char *key_hostaddr = "hostaddr";
    char *key_port = "port";
    char *key_user = "user";
    char *key_password = "password";
    char *key_dbname = "dbname";

    // Here set the keywords params names array

    const char *keywords[] = {key_hostaddr,
                             key_port,
                             key_user,
                             key_password,
                             key_dbname};

    char *fmt = "%s %u %s %s %s";

    if(sscanf(db_name, fmt, hostaddr, &port, dbname, user, password) != 5){
        return SCH_ST_ERROR;
    }

    char *port_str;
    double port_double = 1.0 * port;

    size_t port_str_size = log10(port_double);
    const char *values[] = {
            hostaddr,
            port_str,
            user,
            password,
            dbname
    };

    conn = PQconnectdbParams(keywords, values);

    // Error handling
    if (PQstatus(conn) == CONNECTION_BAD){
        PQfinish(conn);
        return SCH_ST_ERROR;
    }
    else if (PQstatus(conn) == CONNECTION_OK) {
        storage_is_open = 1;
        return SCH_ST_OK;
    }

}

int storage_close(void)
{
    if (conn != NULL){
        PGfinish(conn);
        conn = NULL;
        storage_is_open = 0;
        return SCH_ST_OK;
    }

    if(fp_table != NULL) free(fp_table);
    return SCH_ST_ERROR;
}

int storage_table_status_init(char *table, int n_variables, int drop)
{
    char *err_msg;
    char *sql_stmt;
    int rc;

    char *stmt_name = "drop_on";

    if(drop)
    {
        sql_stmt = PQprepare(conn,
                             stmt_name,
                             "DROP TABLE $1",
                             1,
                             )
    }
}