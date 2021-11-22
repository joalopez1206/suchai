/*                                 SUCHAI
 *                      NANOSATELLITE FLIGHT SOFTWARE
 *
 *      Copyright 2021, Carlos Gonzalez Cortes, carlgonz@uchile.cl
 *      Copyright 2021, Tomas Opazo Toro, tomas.opazo.t@gmail.com
 *      Copyright 2021, Camilo Rojas Milla, camrojas@uchile.cl
 *      Copyright 2021, Matias Ramirez Martinez, nicoram.mt@gmail.com
 *      Copyright 2021, Tamara Gutierrez Rojo tamigr.2293@gmail.com
 *      Copyright 2021, Diego Ortego Prieto, diortego@dcc.uchile.cl
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "suchai/repoData.h"

static const char *tag = "repoData";

dat_stmachine_t status_machine;

void dat_repo_init(void)
{
    // Init repository mutex
    if(osSemaphoreCreate(&repo_data_sem) != OS_SEMAPHORE_OK)
        LOGE(tag, "Unable to create system status repository mutex");
    int rc;

    //Init storage system
    LOGD(tag, "Initializing data repositories buffers...")
    char fs_db_file[strlen(SCH_STORAGE_FILE) + 10];
    sprintf(fs_db_file, "%s.%u.db",SCH_STORAGE_FILE, SCH_COMM_NODE);
    rc = storage_init(fs_db_file);
    if(rc != SCH_ST_OK)
        LOGE(tag, "Unable to initialize data storage! (mode %d, db: %s)", SCH_STORAGE_MODE, fs_db_file);

    //Init status repo
    int status_copies = 1;
#if SCH_STORAGE_TRIPLE_WR
    status_copies = 3;
#endif
    rc = storage_table_status_init(DAT_TABLE_STATUS, status_copies*dat_status_last_var, 0);
    if(rc != SCH_ST_OK)
    {
        LOGE(tag, "Unable to create STATUS repository!. (table %s, len: %d, drop: %d)",
             DAT_TABLE_STATUS, status_copies*dat_status_last_var, 0);
    }
#if SCH_STORAGE_MODE == SCH_ST_RAM
    else // Reset variables (we do not have persistent storage here)
    {
        int index;
        for(index=0; index < dat_status_last_address; index++)
        {
            dat_set_status_var(index, dat_get_status_var_def(index).value);
        }
    }
#endif

    //Init payloads repo
    rc = storage_table_payload_init(DAT_TABLE_DATA, data_map, last_sensor, 0);
    if(rc != SCH_ST_OK)
        LOGE(tag, "Unable to create PAYLOAD repository!. (table %s, len: %d, drop: %d)",
             DAT_TABLE_DATA, last_sensor, 0);

    //Init system flight plan table
    rc = storage_table_flight_plan_init(DAT_TABLE_FP, SCH_FP_MAX_ENTRIES, 0);
    if(rc != SCH_ST_OK)
    {
        LOGE(tag, "Unable to create FLIGHT-PLAN repository!. (table %s, len: %d, drop: %d)",
             DAT_TABLE_FP, SCH_FP_MAX_ENTRIES, 0);
    }
}

void dat_repo_close(void)
{
    storage_close();
}

/**
 * Function for testing triple writing.
 *
 * Should do the same as @c dat_set_system_var , but with only one system status repo.
 *
 * @param index Enum index of the field to set
 * @param value Integer value to set the variable to
 */
int _dat_set_system_var(dat_status_address_t index, int value)
{
    int rc = 0;
    //Enter critical zone
    osSemaphoreTake(&repo_data_sem, portMAX_DELAY);
    value32_t var = {.i=value};
    rc = storage_status_set_value_idx(index, var, DAT_TABLE_STATUS);
    //Exit critical zone
    osSemaphoreGiven(&repo_data_sem);

    return rc;
}

/**
 * Function for testing triple writing.
 *
 * Should do the same as @c dat_get_system_var , but with only one system status repo.
 *
 * @param index Enum index of the field to get
 * @return The field's value
 */
int _dat_get_system_var(dat_status_address_t index)
{
    value32_t value;

    //Enter critical zone
    osSemaphoreTake(&repo_data_sem, portMAX_DELAY);
    storage_status_get_value_idx(index, &value, DAT_TABLE_STATUS);
    //Exit critical zone
    osSemaphoreGiven(&repo_data_sem);

    return value.i;
}

///< Compatibility function
int dat_set_system_var(dat_status_address_t index, int value)
{
    value32_t v;
    v.i = value;
    return dat_set_status_var(index, v);
}

int dat_set_status_var(dat_status_address_t index, value32_t value)
{
    int rc = 0;
    //Enter critical zone
    osSemaphoreTake(&repo_data_sem, portMAX_DELAY);

    rc = storage_status_set_value_idx(index, value, DAT_TABLE_STATUS);
    //Uses tripled writing
    #if SCH_STORAGE_TRIPLE_WR == 1
    int rc2 = storage_status_set_value_idx(index + dat_status_last_address, value, DAT_TABLE_STATUS);
    int rc3 = storage_status_set_value_idx(index + dat_status_last_address*2, value, DAT_TABLE_STATUS);
    rc = rc | rc2 | rc3;
    #endif

    //Exit critical zone
    osSemaphoreGiven(&repo_data_sem);

    return rc;
}

int dat_set_status_var_name(char *name, value32_t value)
{
    dat_sys_var_t var = dat_get_status_var_def_name(name);
    if(var.status == -1)
        return -1; // Value not found
    return dat_set_status_var(var.address, value);
}

///< Compatibility function
int dat_get_system_var(dat_status_address_t index)
{
    value32_t var = dat_get_status_var(index);
    return var.i;
}

value32_t dat_get_status_var(dat_status_address_t index)
{
    //Enter critical zone
    osSemaphoreTake(&repo_data_sem, portMAX_DELAY);

    value32_t value_1;
    storage_status_get_value_idx(index, &value_1, DAT_TABLE_STATUS);
    //Uses tripled writing
    #if SCH_STORAGE_TRIPLE_WR == 1
    value32_t value_2;
    value32_t value_3;
    storage_status_get_value_idx(index+dat_status_last_var, &value_2, DAT_TABLE_STATUS);
    storage_status_get_value_idx(index+dat_status_last_var*2, &value_3, DAT_TABLE_STATUS);
    #endif

    //Exit critical zone
    osSemaphoreGiven(&repo_data_sem);
    
    // Compare values in tripled reading
#if SCH_STORAGE_TRIPLE_WR == 1
    //Compare value and its copies
    if (value_1.u == value_2.u || value_1.u == value_3.u)
        return value_1;
    else if (value_2.u == value_3.u)
        return value_2;
    else
        LOGE(tag, "Unable to get a correct value for index %d", index);
#endif
    return value_1;
}

value32_t dat_get_status_var_name(char *name)
{
    dat_sys_var_t var = dat_get_status_var_def_name(name);
    return dat_get_status_var(var.address);
}

int dat_set_fp(int timetodo, char* command, char* args, int executions, int periodical)
{
    int entries = dat_get_system_var(dat_fpl_queue);

    osSemaphoreTake(&repo_data_sem, portMAX_DELAY);
    //Enter critical zone
    int rc = storage_flight_plan_set(timetodo, command, args, executions, periodical, SCH_COMM_NODE);
    //Exit critical zone
    osSemaphoreGiven(&repo_data_sem);
    if(rc == SCH_ST_OK)
        dat_set_system_var(dat_fpl_queue, entries+1);
    else
        LOGE(tag, "Cannot put FP entry (time: %d, entries %d, cmd %s)", timetodo, entries, command);
    return rc;
}

int dat_get_fp(int elapsed_sec, char* command, char* args, int* executions, int* period)
{
    int rc, node;
    int entries = dat_get_system_var(dat_fpl_queue);
    osSemaphoreTake(&repo_data_sem, portMAX_DELAY);
    //Enter critical zone
    rc = storage_flight_plan_get_args(elapsed_sec, command, args, executions, period, &node);
    if(rc == SCH_ST_OK)
        rc = storage_flight_plan_delete_row(elapsed_sec);
    else
        LOGV(tag, "Cannot read FP entry (time: %d, entries %d)", elapsed_sec, entries);
    //Exit critical zone
    osSemaphoreGiven(&repo_data_sem);

    if(rc == SCH_ST_OK)
        dat_set_system_var(dat_fpl_queue, --entries);

    return rc;
}

int dat_del_fp(int timetodo)
{
    int entries = dat_get_system_var(dat_fpl_queue);
    osSemaphoreTake(&repo_data_sem, portMAX_DELAY);
    //Enter critical zone
    int rc = storage_flight_plan_delete_row(timetodo);
    //Exit critical zone
    osSemaphoreGiven(&repo_data_sem);
    if(rc == SCH_ST_OK)
        dat_set_system_var(dat_fpl_queue, --entries);

    return rc;
}

int dat_reset_fp(void)
{
    int rc;
    osSemaphoreTake(&repo_data_sem, portMAX_DELAY);
    //Enter critical zone
    rc = storage_flight_plan_reset();
    //Exit critical zone
    osSemaphoreGiven(&repo_data_sem);
    if(rc == SCH_ST_OK)
        dat_set_system_var(dat_fpl_queue, 0);
    return rc;
}

int dat_purge_fp(void)
{
    int i, fp_entries = 0;
    int time_min = (int)dat_get_time()+1;
    fp_entry_t fp_i;
    for(i=0; i<SCH_FP_MAX_ENTRIES; i++)
    {
        int ok = storage_flight_plan_get_idx(i, &fp_i);
        if(ok == SCH_ST_OK && fp_i.unixtime > time_min)  // Count valid entries
            fp_entries ++;
        else if(fp_i.unixtime != ST_FP_NULL)    // Delete old entries
            storage_flight_plan_delete_row_idx(i);
    }
    dat_set_system_var(dat_fpl_queue, fp_entries);

    return SCH_ST_OK;
}

int dat_show_fp (void)
{
    int i;
    int rc=SCH_ST_OK;
    char buffer[80];

    osSemaphoreTake(&repo_data_sem, portMAX_DELAY);
    //Enter critical zone
    LOGR(tag,"Time\tCommand\tArguments\tExecutions\tPeriodical\tNode");
    for(i = 0; i < SCH_FP_MAX_ENTRIES; i++)
    {
        fp_entry_t fp_i;
        int ok = storage_flight_plan_get_idx(i, &fp_i);
        if(ok == SCH_ST_OK && fp_i.unixtime != ST_FP_NULL)
        {
            time_t time_to_show = fp_i.unixtime;
            strftime(buffer, 80, "%Y-%m-%d %H:%M:%S UTC", gmtime(&time_to_show));
            LOGR(tag, "%s\t%s\t%s\t%d\t%d\t%d\n", buffer, fp_i.cmd, fp_i.args, fp_i.executions, fp_i.periodical, fp_i.node);
        }
    }
    //Exit critical zone
    osSemaphoreGiven(&repo_data_sem);
    return rc;
}

time_t dat_get_time(void)
{
    return (time_t)osGetTimeUnix();
}

int dat_set_time(int new_time)
{
    return osSetTimeUnix((int64_t)new_time);
}

int dat_show_time(int format)
{
    time_t time_to_show = dat_get_time();

    if(format == 0 || format > 1)
    {
        char buffer[80];
        strftime(buffer, 80, "%Y-%m-%d %H:%M:%S UTC\n", gmtime(&time_to_show));
        LOGR(tag, "%s", buffer);
    }
    if(format >= 1)
    {
        LOGR(tag, "%ld", time_to_show);
    }

    return 0;
}

int dat_add_payload_sample(void* data, int payload)
{
    if(data == NULL)
        return -1;

    int ret;
    int index = dat_get_system_var(data_map[payload].sys_index);
    uint32_t sample = *(uint32_t *)data;
    LOGI(tag, "Adding sample %d for payload %d in index %d", sample, payload, index);

    if(payload >= last_sensor) return SCH_ST_ERROR;

    //Enter critical zone
    osSemaphoreTake(&repo_data_sem, portMAX_DELAY);
    ret = storage_payload_set_data(payload, index, data, &data_map[payload]);
    //Exit critical zone
    osSemaphoreGiven(&repo_data_sem);

    // Update index
    if (ret >= SCH_ST_OK)
    {
        index += 1;
        dat_set_system_var(data_map[payload].sys_index, index);
        return 0;
    }
    else
    {
        LOGE(tag, "Couldn't set data (payload %d, index %d, ret %d)", payload, index, ret);
        return -1;
    }
}

int dat_get_payload_sample(void*data, int payload, int index)
{
    osSemaphoreTake(&repo_data_sem, portMAX_DELAY);
    //TODO: Should we check if the index is valid?
    int ret = storage_payload_get_data(payload, index, data, &data_map[payload]);
    osSemaphoreGiven(&repo_data_sem);

    return ret;
}


int dat_get_recent_payload_sample(void* data, int payload, int offset)
{
    int ret;
    int index = dat_get_system_var(data_map[payload].sys_index);
    LOGV(tag, "Obtaining data of payload %d, in index %d, sys_var: %d", payload, index,data_map[payload].sys_index );

    //Enter critical zone
    osSemaphoreTake(&repo_data_sem, portMAX_DELAY);
    if(index-1-offset >= 0) {
        ret = storage_payload_get_data(payload, index-1-offset, data, &data_map[payload]);
    }
    else {
        LOGE(tag, "Asked for too large offset (%d) on payload (%d)", offset, payload);
        ret = -1;
    }
    //Exit critical zone
    osSemaphoreGiven(&repo_data_sem);

    return ret;
}

int dat_delete_memory_sections(void)
{
    int ret;

    //Free memory or drop databases
    osSemaphoreTake(&repo_data_sem, portMAX_DELAY);
    //Enter critical zone
    ret = storage_payload_reset();
    ret = storage_flight_plan_reset();
    //Exit critical zone
    osSemaphoreGiven(&repo_data_sem);

    // Resetting memory system vars
    for(int i = 0; i < last_sensor; ++i)
        dat_set_system_var(data_map[i].sys_index, 0);
    dat_set_system_var(dat_fpl_queue, 0);

    return ret;
}

int dat_print_payload_struct(void* data, unsigned int payload)
{
    char *types = strdup(data_map[payload].data_order);
    char *names = strdup(data_map[payload].var_names);

    const char *sep = " ";
    char *type_tmp, *name_tmp;
    char *type = strtok_r(types, sep, &type_tmp);
    char *name = strtok_r(names, sep, &name_tmp);
    char *tmp;
    while(type != NULL && name != NULL)
    {
        printf("%s: ", name);
        switch (type[1]) {
            case 'f':
                printf(type, *(float *)data);
                data += sizeof(float);
                break;
            case 'u':
            case 'i':
            case 'd':
                printf(type, *(int32_t *)data);
                data += sizeof(int32_t);
                break;
            case 'h':
                printf("%hi", *(int16_t *)data);
                data += sizeof(int16_t);
                break;
            case 's':
                tmp = strndup((char *)data, SCH_ST_STR_SIZE);
                printf(type, tmp);
                free(tmp);
                data += SCH_ST_STR_SIZE;
                break;
            default:
                data ++;
                break;
        }
        printf("\r\n");

        type = strtok_r(NULL, sep, &type_tmp);
        name = strtok_r(NULL, sep, &name_tmp);
    }

    free(types);
    free(names);

    return 0;
}

int dat_fprint_payload_struct(FILE *stream, void* data, unsigned int payload)
{
    if(stream == NULL)
        return -1;

    char *types = strdup(data_map[payload].data_order);
    const char *sep = " ";
    char *type_tmp;
    char *type = strtok_r(types, sep, &type_tmp);
    char *tmp;
    while(type != NULL)
    {
        switch (type[1]) {
            case 'f':
                fprintf(stream, type, *(float *)data);
                data += sizeof(float);
                break;
            case 'u':
            case 'i':
            case 'd':
                fprintf(stream, type, *(int32_t *)data);
                data += sizeof(int32_t);
                break;
            case 'h':
                fprintf(stream, "%hi", *(int16_t *)data);
                data += sizeof(int16_t);
                break;
            case 's':
                tmp = strndup((char *)data, SCH_ST_STR_SIZE);
                fprintf(stream, type, tmp);
                free(tmp);
                data += SCH_ST_STR_SIZE;
                break;
            default:
                data ++;
                break;
        }
        fprintf(stream, ",");
        type = strtok_r(NULL, sep, &type_tmp);
    }
    fprintf(stream, "\n");
    free(types);
    return 0;
}

int dat_set_stmachine_state(dat_stmachine_action_t action, unsigned int step, int nsamples)
{
    LOGI(tag, "Changing state to  %d %u %d", action, step, nsamples);
    if (action >= 0 && action < ACT_LAST && step > 0 && nsamples > -2) {
        osSemaphoreTake(&repo_machine_sem, portMAX_DELAY);
        status_machine.action = action;
        status_machine.step = step;
        status_machine.samples_left = nsamples;
        osSemaphoreGiven(&repo_machine_sem);
        return 1;
    }
    return 0;
}

int dat_stmachine_is_sensor_active(int payload, int active_payloads, int n_payloads) {
//    printf("max number of active payload %d\n", (1 << n_payloads));
    if ( active_payloads >= (1 << n_payloads) ) {
        return 0;
    }
    return ( (active_payloads & (1 << payload)) != 0 );
}

void _get_sat_quaterion(quaternion_t *q,  dat_status_address_t index)
{
    assert(index+4 < dat_status_last_address);
    int i;
    for(i=0; i<4; i++)
    {
        value32_t v = dat_get_status_var(index+i);
        q->q[i] = (double)v.f;
    }
}

void _set_sat_quaterion(quaternion_t *q,  dat_status_address_t index)
{
    assert(index+4 < dat_status_last_address);
    int i;
    for(i=0; i<4; i++)
    {
        value32_t v;
        v.f = (float)q->q[i];
        dat_set_status_var(index + i, v);
    }
}

void _get_sat_vector(vector3_t *r, dat_status_address_t index)
{
    assert(index+3 < dat_status_last_address);
    int i;
    for(i=0; i<3; i++)
    {
        value32_t v= dat_get_status_var(index+i);
        r->v[i] = (double)v.f;
    }
}

void _set_sat_vector(vector3_t *r, dat_status_address_t index)
{
    assert(index+3 < dat_status_last_address);
    int i;
    for(i=0; i<3; i++)
    {
        value32_t v;
        v.f = (float)r->v[i];
        dat_set_status_var(index + i, v);
    }
}