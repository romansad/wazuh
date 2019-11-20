/* Copyright (C) 2015-2019, Wazuh Inc.
 * All right reserved.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 3) as published by the FSF - Free Software
 * Foundation
 */

#include "mitre.h"

static OSHash *mitre_table;

int mitre_load(char * mode){
    int result = 0;
    int hashcheck;
    int i;
    int j;
    int size_ids;
    int size_tactics;
    char path_db[PATH_MAX + 1];
    char *wazuhdb_query = NULL;
    char *response = NULL;
    char *ext_id = NULL;
    cJSON *root = NULL;
    cJSON *ids = NULL;
    cJSON *id = NULL;
    cJSON *tactics_array = NULL;
    cJSON *tactics_json = NULL;
    cJSON *tactics = NULL;
    cJSON *tactic = NULL;

    snprintf(path_db, sizeof(path_db), "%s/%s.db", WDB_DIR, WDB_MITRE_NAME);

    /* Create hash table */
    mitre_table = OSHash_Create();
    
    /* Get Mitre IDs from Mitre's database */
    os_calloc(OS_SIZE_6144 + 1, sizeof(char), wazuhdb_query);
    snprintf(wazuhdb_query, OS_SIZE_6144, "mitre sql SELECT id from attack;");
    if (result = wdb_send_query(wazuhdb_query, &response), result == -2) {
        merror("Unable to connect to socket '%s'.", WDB_LOCAL_SOCK);
        goto end;
    }

    if (result == -1) {
        merror("No response or bad response from wazuh-db: %s", response);
        goto end;
    }

    /* Parse IDs string */
    const char *jsonErrPtr;
    if (root = cJSON_ParseWithOpts(response+3, &jsonErrPtr, 0), !root) {
        merror("Response from the Mitre database cannot be parsed: %s", response);
        result = -1;
        goto end;
    }

    /* Response parameter has to be freed before continuing */
    os_free(response);

    /* Getting array size */
    if (size_ids = cJSON_GetArraySize(root), size_ids == 0) {
        merror("Response from the Mitre database has 0 elements.");
        result = -1;
        goto end;
    }

    for (i = 0; i < size_ids; i++){
        /* Getting Mitre attack ID from Mitre's database in Wazuh-DB  */
        ids = cJSON_GetArrayItem(root, i);
        if (id = cJSON_GetObjectItem(ids,"id"), id == NULL) {
            merror("It was not possible to get Mitre techniques information.");
            result = -1;
            goto end; 
        }
        ext_id = id->valuestring;

        /* Consulting Mitre's database to get Tactics */
        snprintf(wazuhdb_query, OS_SIZE_6144, "mitre sql SELECT phase_name FROM has_phase WHERE attack_id = '%s';", ext_id);
        if (result = wdb_send_query(wazuhdb_query, &response), result == -2) {
            merror("Unable to connect to socket '%s'.", WDB_LOCAL_SOCK);
            goto end;
        }

        if (result == -1) {
            merror("No response or bad response from wazuh-db: %s", response);
            goto end;
        }

        /* Getting tactics from Mitre's database in Wazuh-DB */
        tactics_array = cJSON_CreateArray();
        if (tactics_json = cJSON_ParseWithOpts(response+3, &jsonErrPtr, 0), !tactics_json) {
            merror("Response from the Mitre database cannot be parsed: %s", response);
            result = -1;
            goto end;
        }
        os_free(response);
        if (size_tactics = cJSON_GetArraySize(tactics_json), size_tactics == 0) {
            merror("Response from the Mitre database has 0 elements.");
            result = -1;
            goto end;
        }
        for (j = 0; j < size_tactics; j++) {
            tactics = cJSON_GetArrayItem(tactics_json, j);
            if (tactic = cJSON_GetObjectItem(tactics,"phase_name"), tactic == NULL) {
                merror("It was not possible to get MITRE tactics information.");
                result = -1;
                goto end;
            }
            cJSON_AddItemToArray(tactics_array, cJSON_Duplicate(tactic,1));
        }
        cJSON_Delete(tactics_json);
        tactics_json = NULL;

        /* Filling Hash table with Mitre's information */
        if (hashcheck = OSHash_Add(mitre_table, ext_id, tactics_array), hashcheck == 0) {
            merror("Mitre Hash table adding failed. Mitre Technique ID '%s' cannot be stored.", ext_id);
            result = -1;
            goto end;
        }

        if (mode != NULL && !strcmp(mode,"test")) {
            cJSON_Delete(tactics_array);
            tactics_array = NULL;
        }
    }

end:
    if (mode != NULL && !strcmp(mode,"test")) {
        OSHash_Free(mitre_table);
    }
    os_free(wazuhdb_query);
    if (response != NULL) {
        os_free(response);    
    }
    if (root != NULL) {
        cJSON_Delete(root);
    }
    if (tactics_json && tactics_json != NULL) {
        cJSON_Delete(tactics_json);
    }
    if (tactics_array && tactics_array != NULL && result != 0) {
        cJSON_Delete(tactics_array);
    }
    if (result != 0) {
         merror("Mitre matrix information could not be loaded.");
    }
    return result;
}

cJSON * mitre_get_attack(const char * mitre_id) {
    return OSHash_Get(mitre_table, mitre_id);
}