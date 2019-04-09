/**********************************************************************************************************
 * Software License Agreement (BSD License)                                                               *
 * Author: Thomas Klausner <tk@giga.or.at>                                                                *
 *                                                                                                        *
 * Copyright (c) 2016, 2017, 2019 Thomas Klausner                                                         *
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
 * Extend a dictionary using data from a JSON file.
 *
 */
#include <freeDiameter/extension.h>
#include <json/json.h>
#include <json/SchemaValidator.h>
#include <sys/stat.h>

extern const char *dict_json_dict_schema;

static int
make_type(const char *typestring, struct dict_object **type)
{
        *type = NULL;

        if (strcmp("Unsigned32", typestring) == 0)
                return AVP_TYPE_UNSIGNED32;
        else if (strcmp("Enumerated", typestring) == 0 || strcmp("Integer32", typestring) == 0)
                return AVP_TYPE_INTEGER32;
        else if (strcmp("OctetString", typestring) == 0)
                return AVP_TYPE_OCTETSTRING;
        else if (strcmp("Grouped", typestring) == 0)
                return AVP_TYPE_GROUPED;
        else if (strcmp("Integer64", typestring) == 0)
                return AVP_TYPE_INTEGER64;
        else if (strcmp("Unsigned64", typestring) == 0)
                return AVP_TYPE_UNSIGNED64;
        else if (strcmp("Float32", typestring) == 0)
                return AVP_TYPE_FLOAT32;
        else if (strcmp("Float64", typestring) == 0)
                return AVP_TYPE_FLOAT64;
        else {
                if (fd_dict_search(fd_g_config->cnf_dict, DICT_TYPE, TYPE_BY_NAME, typestring, type, ENOENT) != 0) {
                        LOG_E("Unknown type '%s'", typestring);
                        return -1;
                }
                return AVP_TYPE_OCTETSTRING;
        }

        return -1;
}


static uint8_t
make_avp_flags(const char *flagstring)
{
        uint8_t flags = 0;

        if (strchr(flagstring, 'M') != NULL)
                flags |= AVP_FLAG_MANDATORY;
        if (strchr(flagstring, 'V') != NULL)
                flags |= AVP_FLAG_VENDOR;

        return flags;
}

static uint8_t
make_command_flags(const char *flagstring)
{
        uint8_t flags = 0;

        if (strchr(flagstring, 'E') != NULL)
                flags |= CMD_FLAG_ERROR;
        if (strchr(flagstring, 'P') != NULL)
                flags |= CMD_FLAG_PROXIABLE;
        if (strchr(flagstring, 'R') != NULL)
                flags |= CMD_FLAG_REQUEST;

        return flags;
}


static bool
add_applications(const Json::Value &config)
{
        Json::Value applications = config["Applications"];
        if (applications == Json::Value::null)
                return true;
        for (Json::ArrayIndex i=0; i<applications.size(); i++) {
                int ret;
                struct dict_application_data application_data;
                application_data.application_id = applications[i]["Code"].asUInt();
                application_data.application_name = (char *)(void *)applications[i]["Name"].asCString();
                if ((ret=fd_dict_new(fd_g_config->cnf_dict, DICT_APPLICATION, &application_data, NULL, NULL)) != 0) {
                        LOG_E("error adding Application '%s' to dictionary: %s", applications[i]["Name"].asCString(), strerror(ret));
                        return false;
                }
                LOG_D("Added Application '%s' to dictionary", applications[i]["Name"].asCString());
        }

        return true;
}

static bool
add_enumtype(const char *enumtypename, enum dict_avp_basetype basetype, struct dict_object **enumtype)
{
        struct dict_type_data data;
        int ret;

        memset(&data, 0, sizeof(data));
        data.type_base = basetype;
        data.type_name = (char *)(void *)enumtypename;
        data.type_interpret = NULL;
        data.type_encode = NULL;
        data.type_dump = NULL;
        if ((ret=fd_dict_new(fd_g_config->cnf_dict, DICT_TYPE, &data, NULL, enumtype)) != 0) {
                /* TODO: allow ret == EEXIST? */
                LOG_E("error defining type '%s': %s", enumtypename, strerror(ret));
                return false;
        }
        LOG_D("Added enumerated type '%s' to dictionary", enumtypename);

        return true;
}

static unsigned int
hexchar(char input) {
        if (input >= '0' and input <= '9')
                return input - '0';
        if (input >= 'a' and input <= 'f')
                return input - 'a' + 10;
        if (input >= 'A' and input <= 'F')
                return input - 'a' + 10;

        return 0;
}

static char
hexdecode(const char *input) {
        return (char)(hexchar(input[0])*16 + hexchar(input[1]));
}

static char *
unpack_enumval_os(const Json::Value &enumvalue, unsigned int &len)
{
        if (!enumvalue.isString()) {
                len = 0;
                return NULL;
        }
        len = enumvalue.asString().size();
        if (enumvalue.asString()[0] == '<' && enumvalue.asString()[len-1] == '>') {
                char *retstr;
                len = len/2 - 1;
                if ((retstr = (char *)malloc(len)) == NULL) {
                        return retstr;
                }
                for (size_t i=0; i<len; i++) {
                        retstr[i] = hexdecode(enumvalue.asCString()+2*i+1);
                }
                return retstr;
        }

        return strdup(enumvalue.asCString());
}

static bool
add_enumvalue(const Json::Value &enumvalue, enum dict_avp_basetype basetype, struct dict_object *enumtype, const char *avpname)
{
        struct dict_enumval_data data;
        int ret;
        bool free_os = false;

        memset(&data, 0, sizeof(data));
        data.enum_name = (char *)(void *)enumvalue["Name"].asCString();
        switch (basetype) {
        case AVP_TYPE_INTEGER32:
                if (enumvalue["Code"].isInt()) {
                        data.enum_value.i32 = enumvalue["Code"].asInt();
                } else {
                        LOG_E("unsupported EnumValue '%s' for AVP '%s', base type Integer32", enumvalue["Name"].asCString(), avpname);
                        return false;
                }
                break;
        case AVP_TYPE_UNSIGNED32:
                if (enumvalue["Code"].isInt()) {
                        data.enum_value.u32 = enumvalue["Code"].asInt();
                } else {
                        LOG_E("unsupported EnumValue '%s' for AVP '%s', base type Unsigned32", enumvalue["Name"].asCString(), avpname);
                        return false;
                }
                break;
        case AVP_TYPE_OCTETSTRING:
                if (enumvalue["Code"].isString()) {
                        unsigned int len;
                        data.enum_value.os.data = (unsigned char *)unpack_enumval_os(enumvalue["Code"], len);
                        data.enum_value.os.len = len;
                        free_os = true;
                } else {
                        LOG_E("unsupported EnumValue '%s' for AVP '%s', base type OctetString", enumvalue["Name"].asCString(), avpname);
                        return false;
                }
                break;
        /* TODO: add other cases when we find that they occur */
        default:
                LOG_E("unsupported EnumValue type '%s' for AVP '%s', type %d", enumvalue["Name"].asCString(), avpname, (int)basetype);
                return false;
        }

        if ((ret=fd_dict_new(fd_g_config->cnf_dict, DICT_ENUMVAL, &data, enumtype, NULL)) != 0) {
                LOG_E("error adding EnumValue '%s' for AVP '%s' to dictionary: %s", enumvalue["Name"].asCString(), avpname, strerror(errno));
                return false;
        }
        LOG_D("Added enumerated value '%s' to dictionary", enumvalue["Name"].asCString());

        if (free_os) {
                free(data.enum_value.os.data);
        }

        return true;
}

static bool
add_avp(const Json::Value &avp)
{
        int bt;
        struct dict_avp_data data;
        struct dict_object *type = NULL;
        struct dict_avp_request dar;
        struct dict_object *existing_avp_obj;
        int fds_ret;

        if (!avp["Code"] || !avp["Name"] || !avp["Type"]) {
                LOG_E("invalid AVP, need Code, Name, and Type");
                return false;
        }

        memset(&data, 0, sizeof(data));

        data.avp_code = avp["Code"].asUInt();
        data.avp_vendor = 0;
        data.avp_name = (char *)(void *)avp["Name"].asCString();
        data.avp_flag_mask = 0;
        if (avp["Flags"]["Must"] != Json::Value::null) {
                data.avp_flag_mask = data.avp_flag_val = make_avp_flags(avp["Flags"]["Must"].asCString());
        }
        if (avp["Flags"]["MustNot"] != Json::Value::null) {
                data.avp_flag_mask |= make_avp_flags(avp["Flags"]["MustNot"].asCString());
        }
        if (avp["Vendor"] != Json::Value::null) {
                data.avp_vendor = avp["Vendor"].asUInt();
                if ((data.avp_flag_mask & AVP_FLAG_VENDOR) == 0) {
                        LOG_D("Vendor flag not set for AVP '%s'", data.avp_name);
                        // error out?
                }
                data.avp_flag_mask |= AVP_FLAG_VENDOR;
                data.avp_flag_val |= AVP_FLAG_VENDOR;
        }
        bt = make_type(avp["Type"].asCString(), &type);
        if (bt == -1) {
                LOG_E("Unknown AVP type %s", avp["Type"].asCString());
                return false;
        }
        data.avp_basetype = (enum dict_avp_basetype)bt;

        if (avp["EnumValues"] != Json::Value::null || avp["Type"] == "Enumerated") {
                if (avp["Type"] != "Enumerated") {
                        LOG_D("AVP '%s': EnumValues defined for non-Enumerated type (freeDiameter extension); type is '%s'", avp["Name"].asCString(), avp["Type"].asCString());
                }
                Json::Value enumvalues = avp["EnumValues"];
                std::string enumtypename = "Enumerated(" + avp["Name"].asString() + ")";
                if (!add_enumtype(enumtypename.c_str(), data.avp_basetype, &type)) {
                        LOG_E("Unknown AVP Enum: %s", enumtypename.c_str());
                        return false;
                }
                for (Json::ArrayIndex i=0; i<enumvalues.size(); i++) {
                        if (!add_enumvalue(enumvalues[i], data.avp_basetype, type, avp["Name"].asCString())) {
                                LOG_E("Unknown AVP Enum Value: %s", avp["Name"].asCString());
                                return false;
                        }
                }
        }

        dar.avp_vendor = data.avp_vendor;
        // dar.avp_code = data.avp_code;
        dar.avp_name = data.avp_name;

        fds_ret = fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME_AND_VENDOR, &dar, &existing_avp_obj, ENOENT);
        if (fds_ret == 0) {
                struct dict_avp_data existing_avp;
                if (fd_dict_getval(existing_avp_obj, &existing_avp) < 0) {
                        LOG_E("Cannot get information for AVP '%s' from dictionary", data.avp_name);
                        return false;
                }
                if (data.avp_code != existing_avp.avp_code
//                    || data.avp_vendor != existing_avp.avp_vendor
//                    || strcmp(data.avp_name, existing_avp.avp_name) != 0
//                    || data.avp_flag_mask != existing_avp.avp_flag_mask
                    || data.avp_vendor != existing_avp.avp_vendor
                    || data.avp_basetype != existing_avp.avp_basetype) {
                        LOG_E("AVP with name '%s' but different properties already exists in dictionary (existing vs. new: Code: %d/%d, Flags: %x/%x, Vendor Id: %d/%d, Basetype: %d/%d)", data.avp_name, data.avp_code, existing_avp.avp_code, data.avp_flag_mask, existing_avp.avp_flag_mask, data.avp_vendor, existing_avp.avp_vendor, data.avp_basetype, existing_avp.avp_basetype);
                        return false;
                }
        } else if (fds_ret == ENOENT) {
                switch(fd_dict_new(fd_g_config->cnf_dict, DICT_AVP, &data, type, NULL)) {
                case 0:
                        LOG_D("Added AVP '%s' to dictionary", data.avp_name);
                        break;
                case EINVAL:
                        LOG_E("error adding AVP '%s' to dictionary: invalid data", data.avp_name);
                        return false;
                case EEXIST:
                        LOG_E("error adding AVP '%s' to dictionary: duplicate entry", data.avp_name);
                        return false;
                default:
                        LOG_E("error adding AVP '%s' to dictionary: %s", data.avp_name, strerror(errno));
                        return false;
                }
        } else {
                LOG_E("error looking up AVP '%s' in dictionary: %s", data.avp_name, strerror(errno));
                return false;
        }
            
        return true;
}

static bool
add_avps(const Json::Value &config)
{
        Json::Value avps = config["AVPs"];
        if (avps == Json::Value::null)
                return true;
        for (Json::ArrayIndex i=0; i<avps.size(); i++) {
                if (!add_avp(avps[i])) {
                        LOG_E("error adding AVP to dictionary");
                        return false;
                }
        }

        return true;
}

static bool
look_up_application(const Json::Value &application, struct dict_object **application_object)
{
        fd_dict_search(fd_g_config->cnf_dict, DICT_APPLICATION, APPLICATION_BY_NAME, (void *)application.asCString(), application_object, 0);
        if (*application_object == NULL) {
                LOG_E("Application '%s' not found", application.asCString());
                return false;
        }
        return true;
}

static bool
add_commands(const Json::Value &config)
{
        Json::Value commands = config["Commands"];
        if (commands == Json::Value::null)
                return true;
        for (Json::ArrayIndex i=0; i<commands.size(); i++) {
                int ret;
                struct dict_cmd_data command_data;
                command_data.cmd_code = commands[i]["Code"].asUInt();
                command_data.cmd_name = (char *)(void *)commands[i]["Name"].asCString();
                command_data.cmd_flag_mask = 0;
                command_data.cmd_flag_val = 0;
                if (commands[i]["Flags"]["Must"] != Json::Value::null) {
                        command_data.cmd_flag_mask = command_data.cmd_flag_val = make_command_flags(commands[i]["Flags"]["Must"].asCString());
                }
                if (commands[i]["Flags"]["MustNot"] != Json::Value::null) {
                        command_data.cmd_flag_mask |= make_command_flags(commands[i]["Flags"]["MustNot"].asCString());
                }
                if (commands[i]["Application"] != Json::Value::null) {
                        struct dict_object *application_object;
                        if (!look_up_application(commands[i]["Application"], &application_object)) {
                                LOG_E("Application not found in dictionary");
                                return false;
                        }
                        if ((ret=fd_dict_new(fd_g_config->cnf_dict, DICT_COMMAND, &command_data, application_object, NULL)) != 0) {
                                LOG_E("error adding Command '%s' to dictionary: %s", commands[i]["Name"].asCString(), strerror(ret));
                                return false;
                        }
                } else {
                        if ((ret=fd_dict_new(fd_g_config->cnf_dict, DICT_COMMAND, &command_data, NULL, NULL)) != 0) {
                                LOG_E("error adding Command '%s' to dictionary: %s", commands[i]["Name"].asCString(), strerror(ret));
                                return false;
                        }
                }
                LOG_D("Added Command '%s' to dictionary", commands[i]["Name"].asCString());
        }

        return true;
}

static bool
look_up_avp(const Json::Value &avp, const Json::Value &vendor, struct dict_object **avp_object)
{
        if (vendor != Json::Value::null) {
                struct dict_avp_request_ex dare;
                memset(&dare, 0, sizeof(dare));
                if (vendor.isInt()) {
                        LOG_D("Looking for AVP '%s', Vendor %d", avp.asCString(), vendor.asInt());
                        dare.avp_vendor.vendor_id = vendor.asUInt();
                } else if (vendor.isString()) {
                        LOG_D("Looking for AVP '%s', Vendor '%s'", avp.asCString(), vendor.asCString());
                        dare.avp_vendor.vendor_name = vendor.asCString();
                } else {
                        LOG_E("error finding AVP '%s', invalid value for Vendor", avp.asCString());
                        return false;
                }
                dare.avp_data.avp_name = avp.asCString();
                fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_STRUCT, &dare, avp_object, 0);
        } else {
                fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, (void *)avp.asCString(), avp_object, 0);
        }
        if (*avp_object == NULL) {
                LOG_E("AVP '%s' not found", avp.asCString());
                return false;
        }
        return true;
}

static bool
add_rule_entry(const Json::Value &rule, struct dict_object *parent)
{
        struct dict_rule_data data;
        int ret;

        memset(&data, 0, sizeof(data));
        data.rule_min = -1;
        if (rule["Min"] != Json::Value::null) {
                if (rule["Min"].isInt())
                        data.rule_min = rule["Min"].asInt();
                else if (rule["Min"].isString() && strcmp(rule["Min"].asCString(), "unbounded") == 0)
                        data.rule_min = -1;
                else {
                        LOG_E("error adding rule with AVP '%s', invalid value for Min", rule["AVP"].asCString());
                }
                data.rule_min = rule["Min"].asInt();
        }
        if (data.rule_min > 0) {
                data.rule_position = RULE_REQUIRED;
        } else {
                data.rule_position = RULE_OPTIONAL;
        }
        data.rule_max = -1;
        if (rule["Max"] != Json::Value::null) {
                if (rule["Max"].isInt())
                        data.rule_max = rule["Max"].asInt();
                else if (rule["Max"].isString() && strcmp(rule["Max"].asCString(), "unbounded") == 0)
                        data.rule_max = -1;
                else {
                        LOG_E("error adding rule with AVP '%s', invalid value for Max", rule["AVP"].asCString());
                        return false;
                }
        }
        if (rule["First"] == true) {
                data.rule_position = RULE_FIXED_HEAD;
                data.rule_order = 1;
        }

        LOG_D("Looking up AVP '%s' for rule", rule["AVP"].asCString());
        if (!look_up_avp(rule["AVP"], rule["Vendor"], &data.rule_avp)) {
                LOG_E("Adding rule: AVP '%s' not found", rule["AVP"].asCString());
                return false;
        }
        LOG_D("Found AVP '%s' for rule", rule["AVP"].asCString());
        if ((ret=fd_dict_new(fd_g_config->cnf_dict, DICT_RULE, &data, parent, NULL)) < 0) {
                LOG_E("error adding rule with AVP '%s': %s", rule["AVP"].asCString(), strerror(ret));
                return false;
        }
        LOG_D("Added AVP '%s' to rule", rule["AVP"].asCString());
        return true;
}


static bool
add_command_rule(const Json::Value &rule)
{
        int ret;
        struct dict_object *command;

        if ((ret=fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME, rule["Command"].asCString(), &command, ENOENT)) < 0) {
                LOG_E("Command '%s' not found in dictionary: %s", rule["Command"].asCString(), strerror(ret));
                return false;
        }

        Json::Value content = rule["Content"];
        if (content == Json::Value::null) {
                LOG_E("No rules for Command '%s'", rule["Command"].asCString());
                return false;
        }
        for (Json::ArrayIndex i=0; i<content.size(); i++) {
                if (!add_rule_entry(content[i], command)) {
                        LOG_E("error adding command rule to dictionary");
                        return false;
                }
        }
        LOG_D("Added rules for Command '%s'", rule["Command"].asCString());

        return true;
}

static bool
add_avp_rule(const Json::Value &rule)
{
        struct dict_object *avp;
        struct dict_avp_data user_name_data;

        if (!look_up_avp(rule["AVP"], rule["Vendor"], &avp)) {
                LOG_E("AVP '%s' not found in dictionary", rule["AVP"].asCString());
                return false;
        }

        if (fd_dict_getval(avp, &user_name_data) < 0) {
                LOG_E("Cannot get information for AVP '%s' from dictionary", rule["AVP"].asCString());
                return false;
        }
        if (user_name_data.avp_basetype != AVP_TYPE_GROUPED) {
                LOG_E("Invalid type for AVP '%s': cannot define rule for non-Grouped AVP", rule["AVP"].asCString());
                return false;
        }

        Json::Value content = rule["Content"];
        if (content == Json::Value::null) {
                LOG_E("No rules for AVP '%s'", rule["AVP"].asCString());
                return false;
        }
        for (Json::ArrayIndex i=0; i<content.size(); i++) {
                if (!add_rule_entry(content[i], avp)) {
                        LOG_E("error adding AVP rule to dictionary");
                        return false;
                }
        }
        LOG_D("Added rules for AVP '%s'", rule["AVP"].asCString());

        return true;
}

static bool
add_command_rules(const Json::Value &config)
{
        Json::Value commandrules = config["CommandRules"];
        if (commandrules == Json::Value::null)
                return true;
        for (Json::ArrayIndex i=0; i<commandrules.size(); i++) {
                if (!add_command_rule(commandrules[i])) {
                        LOG_E("error adding command rules to dictionary");
                        return false;
                }
        }

        return true;
}

static bool
add_avp_rules(const Json::Value &config)
{
        Json::Value avprules = config["AVPRules"];
        if (avprules == Json::Value::null)
                return true;
        for (Json::ArrayIndex i=0; i<avprules.size(); i++) {
                if (!add_avp_rule(avprules[i])) {
                        LOG_E("error adding AVP rules to dictionary");
                        return false;
                }
        }

        return true;
}

struct base_types_map {
        std::string name;
        enum dict_avp_basetype value;
};
struct base_types_map base_types[] = {
        { "OctetString", AVP_TYPE_OCTETSTRING },
        { "Integer32", AVP_TYPE_INTEGER32 },
        { "Integer64", AVP_TYPE_INTEGER64 },
        { "Unsigned32", AVP_TYPE_UNSIGNED32 },
        { "Enumerated", AVP_TYPE_INTEGER32 },
        { "Unsigned64", AVP_TYPE_UNSIGNED64 },
        { "Float32", AVP_TYPE_FLOAT32 },
        { "Float64", AVP_TYPE_FLOAT64 }
};

static int
basic_type(std::string name) {
        for (unsigned int i=0; i<sizeof(base_types)/sizeof(base_types[0]); i++) {
                if (name == base_types[i].name)
                        return base_types[i].value;
        }
        return -1;
}

static bool
add_types(const Json::Value &config)
{
        Json::Value types = config["Types"];
        if (types == Json::Value::null)
                return true;
        for (Json::ArrayIndex i=0; i<types.size(); i++) {
                int ret;
                struct dict_type_data type_data;
                struct dict_object *base_type;
                struct dict_type_data base_data;

                ret = fd_dict_search(fd_g_config->cnf_dict, DICT_TYPE, TYPE_BY_NAME, types[i]["Base"].asCString(), &base_type, ENOENT);
                switch (ret) {
                case 0:
                        if (fd_dict_getval(base_type, &base_data) < 0) {
                                LOG_E("Error looking up base type '%s' for type '%s'", types[i]["Base"].asCString(), types[i]["Name"].asCString());
                                return false;
                        }
                        type_data.type_base = base_data.type_base;
                        break;

                case ENOENT:
                        /* not an extended type, perhaps a basic one? */
                        ret = basic_type(types[i]["Base"].asString());
                        if (ret != -1) {
                                type_data.type_base = (enum dict_avp_basetype)ret;
                                break;
                        }
                        /* fallthrough */

                default:
                        /* not found, or error */
                        LOG_E("Base type '%s' for type '%s' not found", types[i]["Base"].asCString(), types[i]["Name"].asCString());
                        return false;
                }

                type_data.type_name = (char *)(void *)types[i]["Name"].asCString();
                if ((ret=fd_dict_new(fd_g_config->cnf_dict, DICT_TYPE, &type_data, NULL, NULL)) != 0) {
                        LOG_E("error adding Type '%s' to dictionary: %s", types[i]["Name"].asCString(), strerror(ret));
                        return false;
                }
                LOG_D("Added Type '%s' to dictionary", types[i]["Name"].asCString());
        }

        return true;
}

static bool
add_vendors(const Json::Value &config)
{
        Json::Value vendors = config["Vendors"];
        if (vendors == Json::Value::null)
                return true;
        for (Json::ArrayIndex i=0; i<vendors.size(); i++) {
                int ret;
                struct dict_vendor_data vendor_data;
                vendor_data.vendor_id = vendors[i]["Code"].asUInt();
                vendor_data.vendor_name = (char *)(void *)vendors[i]["Name"].asCString();
                if ((ret=fd_dict_new(fd_g_config->cnf_dict, DICT_VENDOR, &vendor_data, NULL, NULL)) != 0) {
                        LOG_E("error adding Vendor '%s' to dictionary: %s", vendors[i]["Name"].asCString(), strerror(ret));
                        return false;
                }
                LOG_D("Added Vendor '%s' to dictionary", vendors[i]["Name"].asCString());
        }

        return true;
}

static bool
parse_json_from_file(const char *conffile, Json::Value &jv)
{
        struct stat sb;
        char *buf;
        FILE *fp;
        Json::Reader reader;
        static Json::SchemaValidator *validator = NULL;

        if (conffile == NULL || stat(conffile, &sb) < 0 || !S_ISREG(sb.st_mode)) {
                LOG_E("invalid or missing configuration: %s", conffile ?: "(null)");
                return false;
        }
        if ((buf=(char *)malloc(static_cast<size_t>(sb.st_size + 1))) == NULL) {
                LOG_E("malloc failure of %zu bytes", static_cast<size_t>(sb.st_size));
                return false;
        }
        if ((fp=fopen(conffile, "r")) == NULL) {
                LOG_E("error opening file '%s': %s", conffile, strerror(errno));
                return false;
        }
        if (fread(buf, static_cast<size_t>(sb.st_size), 1, fp) != 1) {
                LOG_E("error reading from file '%s': %s", conffile, strerror(errno));
                return false;
        }
        (void)fclose(fp);

        buf[sb.st_size] = '\0';

        if (!reader.parse (buf, jv)) {
                LOG_E("error parsing JSON data from file '%s': %s", conffile, reader.getFormattedErrorMessages().c_str());
                return false;
        }
        free(buf);

        if (validator == NULL) {
                try {
                        validator = new Json::SchemaValidator(std::string(dict_json_dict_schema));
                } catch (Json::SchemaValidator::Exception &e) {
                        LOG_E("error creating JSON schema validator: %s", e.type_message().c_str());
                        return false;
                }
        }

        if (!validator->validate(jv)) {
                LOG_E("error validating config file %s:", conffile);
                const std::vector<Json::SchemaValidator::Error> &errors = validator->errors();
                for (std::vector<Json::SchemaValidator::Error>::const_iterator it = errors.begin(); it != errors.end(); ++it) {
                        LOG_E("  %s:%s", it->path.c_str(), it->message.c_str());
                }
                return false;
        }

#if 0
        Json::StyledWriter styledWriter;
        std::cout << styledWriter.write(jv);
#endif

        return true;
}

static bool
read_dictionary(const char *filename)
{
        Json::Value config = Json::Value::null;
        if (!parse_json_from_file (filename, config)) {
                LOG_E("error parsing JSON file");
                return false;
        }

        if (!add_vendors(config)) {
                LOG_E("error adding vendors");
                return false;
        }
        if (!add_types(config)) {
                LOG_E("error adding types");
                return false;
        }
        if (!add_avps(config)) {
                LOG_E("error adding AVPs");
                return false;
        }
        if (!add_applications(config)) {
                LOG_E("error adding applications");
                return false;
        }
        if (!add_commands(config)) {
                LOG_E("error adding commands");
                return false;
        }
        if (!add_command_rules(config)) {
                LOG_E("error adding command rules");
                return false;
        }
        if (!add_avp_rules(config)) {
                LOG_E("error adding AVP rules");
                return false;
        }

        return true;
}

static int
dict_json_entry(char * conffile)
{
        Json::Value main_config = Json::Value::null;
        char *filename, *filename_base, *p;

        TRACE_ENTRY("%p", conffile);

        filename_base = strdup(conffile);
        if (filename_base == NULL) {
                LOG_E("error initialising JSON dictionary extension: %s", strerror(errno));
                return 1;
        }
        filename = filename_base;
        while ((p=strsep(&filename, ";")) != NULL) {
                LOG_D("parsing dictionary '%s'", p);
                if (!read_dictionary(p)) {
                        LOG_E("error reading JSON dictionary '%s'", p);
                        return EINVAL;
                }
                LOG_N("loaded JSON dictionary '%s'", p);
                if (filename == NULL)
                        break;
        }

        free(filename_base);
        LOG_N("Extension 'Dictionary definitions from JSON dictionaries' initialized");
        return 0;
}

extern "C" {
        EXTENSION_ENTRY("dict_json", dict_json_entry);
}
