/**********************************************************************************************************
 * Software License Agreement (BSD License)                                                               *
 * Author: Thomas Klausner <tk@giga.or.at>                                                                *
 *                                                                                                        *
 * Copyright (c) 2020 Thomas Klausner                                                                     *
 * All rights reserved.                                                                                   *
 *                                                                                                        *
 * Written under contract by nfotex IT GmbH, http://nfotex.com/                                           *
 *                                                                                                        *
 * Redistribution and use of this software in source and binary forms, with or without modification, are  *
 * permitted provided that the following conditions are met:                                              *
 *                                                                                                        *
 * * Redistributions of source code must retain the above                                                 *
 *   copyright notice, this list of conditions and the                                                    *
 *   following disclaimer.                                                                                *
 *                                                                                                        *
 * * Redistributions in binary form must reproduce the above                                              *
 *   copyright notice, this list of conditions and the                                                    *
 *   following disclaimer in the documentation and/or other                                               *
 *   materials provided with the distribution.                                                            *
 *                                                                                                        *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED *
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A *
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR *
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT     *
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS    *
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF   *
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                                             *
 **********************************************************************************************************/


/*
 * Dump Diameter dictionary to JSON file.
 */

#include <freeDiameter/extension.h>
#include "dictionary-internal.h"
#include <json/json.h>
#include <sys/stat.h>

#include <iomanip>
#include <sstream>


void
dump_vendor(dict_object *self, struct dict_vendor_data *data, struct dict_object *parent, int depth, Json::Value &main)
{
        Json::Value vendor;

        vendor["Code"] = Json::Value(data->vendor_id);
        vendor["Name"] = Json::Value(data->vendor_name);
        main["Vendors"].append(vendor);
}

void
dump_application(dict_object *self, struct dict_application_data *data, struct dict_object *parent, int depth, Json::Value &main)
{
        Json::Value application;

        application["Code"] = Json::Value(data->application_id);
        application["Name"] = Json::Value(data->application_name);
        main["Applications"].append(application);
}

void
dump_enumval(dict_object *self, struct dict_enumval_data *data, struct dict_object *parent, int depth, Json::Value &main)
{
        enum dict_avp_basetype type;
        struct dict_type_data type_data;
        Json::Value enumval;
        bool is_printable;

        type = parent->data.type.type_base;

        /* Standard only allows Integer32 Enumerated values, but freeDiameter is more permissive */
        switch (type) {
        case AVP_TYPE_OCTETSTRING:
                is_printable = true;
                for (size_t i = 0; i < data->enum_value.os.len; i++) {
                        if (isprint(data->enum_value.os.data[i]) == 0) {
                                is_printable = false;
                                break;
                        }
                }
                if (is_printable) {
                        std::string output((const char*)data->enum_value.os.data, (size_t)data->enum_value.os.len);
                        enumval["Code"] = Json::Value(output);
                } else {
                        std::stringstream quoted_string;
                        quoted_string << "<";
                        for (size_t i = 0; i < data->enum_value.os.len; i++) {
                                quoted_string << std::hex << std::setfill('0') << std::setw(2) << (int)data->enum_value.os.data[i];
                        }
                        quoted_string << ">";
                        enumval["Code"] = Json::Value(quoted_string.str());
                }
                break;

        case AVP_TYPE_INTEGER32:
                enumval["Code"] = Json::Value(data->enum_value.i32);
                break;

        case AVP_TYPE_INTEGER64:
                enumval["Code"] = Json::Value(static_cast<Json::Int64>(data->enum_value.i64));
                break;

        case AVP_TYPE_UNSIGNED32:
                enumval["Code"] = Json::Value(data->enum_value.u32);
                break;

        case AVP_TYPE_UNSIGNED64:
                enumval["Code"] = Json::Value(static_cast<Json::UInt64>(data->enum_value.u64));
                break;

        case AVP_TYPE_FLOAT32:
                enumval["Code"] = Json::Value(data->enum_value.f32);
                break;

        case AVP_TYPE_FLOAT64:
                enumval["Code"] = Json::Value(data->enum_value.f64);
                break;

        default:
                printf("??? (ERROR unknown type %d)", type);
                break;
        }
        if (fd_dict_getval(parent, &type_data) != 0) {
                /* TODO: fd_dict_getval error */
                return;
        }
        if (!main.isMember("Types") || !main["Types"].isMember(type_data.type_name)) {
                /* TODO: missing type error */
                return;
        }

        enumval["Name"] = Json::Value(data->enum_name);
        main["Types"][type_data.type_name]["EnumValues"].append(enumval);
}

void
dump_type(dict_object *self, struct dict_type_data *data, struct dict_object *parent, int depth, Json::Value &main)
{
        Json::Value type;

        type["Name"] = Json::Value(data->type_name);
        type["Base"] = Json::Value(type_base_name[data->type_base]);
        main["Types"][data->type_name] = type;
}

#define AVPFL_str "%s%s"
#define AVPFL_val(_val) (_val & AVP_FLAG_VENDOR)?"V":"" , (_val & AVP_FLAG_MANDATORY)?"M":""

void
dump_avp(dict_object *self, struct dict_avp_data *data, struct dict_object *parent, int depth, Json::Value &main)
{
        struct dict_object *type = NULL;
        struct dict_type_data type_data;
        Json::Value avp;
        char flags[10];

        if (fd_dict_search(fd_g_config->cnf_dict, DICT_TYPE, TYPE_OF_AVP, self, &type, 0) != 0) {
                /* TODO: fd_dict_search error */
                return;
	}

        if ((type == NULL) || (fd_dict_getval(type, &type_data) != 0)) {
                avp["Type"] = Json::Value(type_base_name[data->avp_basetype]);
        } else {
                if (strstr(type_data.type_name, "Enumerated") != 0) {
                        if (data->avp_basetype == AVP_TYPE_INTEGER32) {
                                avp["Type"] = "Enumerated";
                        } else {
                                /* freeDiameter allows enumerated types not based on integer32;
                                 * write those out with their basic type for dict_json */
                                avp["Type"] = Json::Value(type_base_name[data->avp_basetype]);
                        }
                } else {
                        avp["Type"] = Json::Value(type_data.type_name);
                }
                if (main["Types"].isMember(type_data.type_name) && main["Types"][type_data.type_name].isMember("EnumValues")) {
                        avp["EnumValues"] = main["Types"][type_data.type_name]["EnumValues"];
                }
        }

        avp["Code"] = Json::Value(data->avp_code);
        if (data->avp_vendor != 0) {
                avp["Vendor"] = Json::Value(data->avp_vendor);
        }
        avp["Name"] = Json::Value(data->avp_name);
        snprintf(flags, sizeof(flags), AVPFL_str, AVPFL_val(data->avp_flag_val));
        avp["Flags"]["Must"] = Json::Value(flags);
        snprintf(flags, sizeof(flags), AVPFL_str, AVPFL_val(data->avp_flag_mask & ~data->avp_flag_val));
        avp["Flags"]["MustNot"] = Json::Value(flags);

        main["AVPs"].append(avp);
}

#define CMDFL_str "%s%s%s%s"
#define CMDFL_val(_val) (_val & CMD_FLAG_REQUEST)?"R":"" , (_val & CMD_FLAG_PROXIABLE)?"P":"" , (_val & CMD_FLAG_ERROR)?"E":"" , (_val & CMD_FLAG_RETRANSMIT)?"T":""

void
dump_command(dict_object *self, struct dict_cmd_data *data, struct dict_object *parent, int depth, Json::Value &main)
{
        Json::Value command;
        char flags[10];

        command["Code"] = Json::Value(data->cmd_code);
        command["Name"] = Json::Value(data->cmd_name);
        snprintf(flags, sizeof(flags), CMDFL_str, CMDFL_val(data->cmd_flag_val));
        command["Flags"]["Must"] = Json::Value(flags);
        snprintf(flags, sizeof(flags), CMDFL_str, CMDFL_val(data->cmd_flag_mask & ~data->cmd_flag_val));
        command["Flags"]["MustNot"] = Json::Value(flags);
        main["Commands"].append(command);
}

bool dump_object(dict_object *self, int depth, Json::Value &main);

void
dump_rule(dict_object *self, struct dict_rule_data *data, struct dict_object *parent, int depth, Json::Value &main)
{
        Json::Value avp_rule;
        struct dict_avp_data avp_data;
        const char *slot, *entry, *parent_avp_name;
        vendor_id_t parent_avp_vendor;
        unsigned int i;
        bool found = false;
        enum dict_object_type parent_type;

        if (fd_dict_getval(data->rule_avp, &avp_data) != 0) {
                /* TODO: fd_dict_getval error */
                return;
        }

        if (avp_data.avp_vendor != 0) {
                avp_rule["Vendor"] = Json::Value(avp_data.avp_vendor);
        }
        avp_rule["AVP"] = Json::Value(avp_data.avp_name);
        /* TODO is this correct? what does rule_order specify? */
        if (data->rule_position == RULE_FIXED_HEAD) {
                avp_rule["First"] = Json::Value(true);
        }
        /* TODO: insert "unbounded" for -1? */
        if (data->rule_min != -1) {
                avp_rule["Min"] = Json::Value(data->rule_min);
        }
        if (data->rule_max != -1) {
                avp_rule["Max"] = Json::Value(data->rule_max);
        }

        int ret = fd_dict_gettype(parent, &parent_type);
        if (ret != 0) {
                /* TODO: fd_dict_gettype error */
                return;
        }

        if (parent_type == DICT_AVP) {
                struct dict_avp_data parent_data;

                slot = "AVPRules";
                entry = "AVP";
                if (fd_dict_getval(parent, &parent_data) != 0) {
                        /* TODO: fd_dict_getval error */
                        return;
                }
                parent_avp_name = parent_data.avp_name;
                parent_avp_vendor = parent_data.avp_vendor;
        } else if (parent_type == DICT_COMMAND) {
                struct dict_cmd_data parent_data;

                slot = "CommandRules";
                entry = "Command";
                if (fd_dict_getval(parent, &parent_data) != 0) {
                        /* TODO: fd_dict_getval error */
                        return;
                }
                parent_avp_name = parent_data.cmd_name;
                parent_avp_vendor = 0;
        } else {
                /* TODO: error, unknown case */
                return;
        }

        for (i=0; i<main[slot].size(); i++) {
                if (main[slot][i][entry] == parent_avp_name
                    && (parent_avp_vendor == 0 || !main[slot][i].isMember("Vendor") || parent_avp_vendor == main[slot][i]["Vendor"].asUInt())) {
                        found = true;
                        main[slot][i]["Content"].append(avp_rule);
                        break;
                }
        }
        if (!found) {
                Json::Value parent_avp;
                parent_avp[entry] = parent_avp_name;
                if (parent_avp_vendor != 0) {
                        parent_avp["Vendor"] = Json::Value(parent_avp_vendor);
                }
                parent_avp["Content"].append(avp_rule);
                main[slot].append(parent_avp);
        }

}

bool
dump_list(struct fd_list *list, const char *type, int depth, Json::Value &main) {
        fd_list *li;
        for (li = list->next; li != list; li = li->next) {
                if (!dump_object((dict_object *)li->o, depth, main)) {
                        LOG_E("error dumping %s", type);
                        return false;
                }
        }
        return true;
}

bool
dump_object(dict_object *self, int depth, Json::Value &main)
{
        enum dict_object_type t;
        int ret = fd_dict_gettype (self, &t);
        if (ret != 0) {
                return false;
        }

        switch (t) {
#define DUMP_OBJECT_CASE(TYPE, STRUCT, DUMP_FUNCTION)                   \
                case TYPE: {                                            \
                        struct STRUCT * data = NULL;                    \
                        int i;                                          \
                        data = (struct STRUCT *)malloc(sizeof(struct STRUCT)); \
                        if (!data) {                                    \
                                /* TODO: malloc error */                \
                                return false;                           \
                        }                                               \
                        ret = fd_dict_getval(self, data);               \
                        if (ret != 0) {                                 \
                                /* TODO: fd_dict_getval error */        \
                                free(data);                             \
                                return false;                           \
                        }                                               \
                        DUMP_FUNCTION(self, data, self->parent, depth, main); \
                        if (depth > 0) {                                \
                                for (i=0; i<NB_LISTS_PER_OBJ; i++) {    \
                                        if ((self->list[i].o == NULL) && (self->list[i].next != &self->list[i])) { \
                                                dump_list(&self->list[i], 0, depth-1, main); \
                                                break;                  \
                                        }                               \
                                }                                       \
                        }                                               \
                }                                                       \
                        break;

                DUMP_OBJECT_CASE(DICT_VENDOR, dict_vendor_data, dump_vendor);
                DUMP_OBJECT_CASE(DICT_APPLICATION, dict_application_data, dump_application);
                DUMP_OBJECT_CASE(DICT_TYPE, dict_type_data, dump_type);
                DUMP_OBJECT_CASE(DICT_ENUMVAL, dict_enumval_data, dump_enumval);
                DUMP_OBJECT_CASE(DICT_AVP, dict_avp_data, dump_avp);
                DUMP_OBJECT_CASE(DICT_COMMAND, dict_cmd_data, dump_command);
                DUMP_OBJECT_CASE(DICT_RULE, dict_rule_data, dump_rule);
        default:
                /* TODO: unhandled type error */
                return false;
        }
        return true;
}

bool
dump_dictionary_to_json(dictionary *dict, Json::Value &main)
{
        if (dict == NULL) {
                return false;
        }

        if (pthread_rwlock_rdlock(&dict->dict_lock) != 0) {
                return false;
        }

        dump_list(&(dict->dict_types), "types", 2, main);
        dump_object(&(dict->dict_vendors), 3, main);
        dump_list(&(dict->dict_vendors.list[0]), "vendors", 3, main);
        dump_object(&(dict->dict_applications), 1, main);
        dump_list(&(dict->dict_applications.list[0]), "applications", 1, main);
        dump_list(&(dict->dict_cmd_code), "commands", 1, main);

        if (pthread_rwlock_unlock( &dict->dict_lock) != 0) {
                return false;
        }

        return true;
}


static int
dbg_dict_dump_json_entry(char * conffile)
{
        TRACE_ENTRY("%p", conffile);
        Json::Value main;
        Json::Value types = Json::Value::null;
        Json::StyledWriter writer;
        FILE *out = stdout;

        if (conffile) {
                if ((out=fopen(conffile, "w")) == NULL) {
                        LOG_E("cannot open output file '%s' for writing", conffile);
                        return EINVAL;
                }
        }

        /* get main dictionary */
        struct dictionary * dict = fd_g_config->cnf_dict;
        if (!dump_dictionary_to_json(dict, main)) {
                LOG_E("error dumping dictionary to JSON");
                return EINVAL;
        }
        /* remove enumerated types before dumping, they are in AVPs */
        /* convert remaining ones to array */
        Json::Value::Members members = main["Types"].getMemberNames();
        for (Json::Value::Members::const_iterator it = members.begin() ; it != members.end(); ++it) {
                if (strncmp("Enumerated", main["Types"][*it]["Name"].asCString(), strlen("Enumerated")) == 0) {
                        main["Types"].removeMember(*it);
                } else {
                        types.append(main["Types"][*it]);
                }
        }
        main.removeMember("Types");
        main["Types"] = types;

        std::string value_str = writer.write(main);
        fprintf(out, "%s\n", value_str.c_str());

        if (conffile) {
                fclose(out);
        }

        LOG_N("Extension 'Dump dictionaries to JSON' initialized");
        return 0;
}

extern "C" {
        EXTENSION_ENTRY("dbg_dict_dump_json", dbg_dict_dump_json_entry);
}
