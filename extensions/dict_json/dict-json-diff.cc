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
 * Show what was added from one dictionary dump JSON file compared to a second one
 */
#include <json/json.h>

#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

void json_compare_arrays(Json::Value &base, Json::Value &big, Json::Value &diff, string name, string key, string key2);

void
json_compare_objects(Json::Value &base, Json::Value &big, Json::Value &diff, string name)
{
    Json::Value::Members members = big[name].getMemberNames();
    for (auto it = members.begin() ; it != members.end(); ++it) {
        if (!base[name].isMember(*it) ||
            (base[name][*it] != big[name][*it])) {
            diff[name][*it] = big[name][*it];
        }
    }
}

void
json_compare_avps(Json::Value &base, Json::Value &big, Json::Value &diff, string name)
{
    Json::Value left = base[name];
    Json::Value right = big[name];

    if (left.type() != right.type() || left.empty()) {
        if (!right.empty()) {
            diff[name] = right;
        }
        return;
    }

    if (!left.isArray()) {
        cerr << name << " member not an array\n";
        return;
    }

    for (unsigned int i=0; i<right.size(); i++) {
        bool found = false;
        /* find matching entry in base */
        for (unsigned int j=0; j<left.size(); j++) {
            Json::Value temp = Json::Value::null;
            if (left[j]["Code"] != right[i]["Code"] ||
                (left[j]["Vendor"] != right[i]["Vendor"])) {
                continue;
            }
            Json::Value::Members members = right[i].getMemberNames();
            for (auto it = members.begin() ; it != members.end(); ++it) {
                if (*it == "Flags") {
                    json_compare_objects(left[j], right[i], temp, *it);
                    continue;
                }
                if (*it == "EnumValues") {
                    std::string none;
                    json_compare_arrays(left[j], right[i], temp, *it, "Code", none);
                    continue;
                }
                if (left[j].isMember(*it)) {
                    if (left[j][*it] != right[i][*it]) {
                        temp[*it] = right[i][*it];
                    }
                } else {
                    temp[*it] = right[i][*it];
                }
            }
            if (!temp.isNull()) {
                temp["Code"] = right[i]["Code"];
                if (!right[i]["Vendor"].isNull()) {
                    temp["Vendor"] = right[i]["Vendor"];
                }
                if (temp.isMember("EnumValues")) {
                    temp["Name"] = right[i]["Name"];
                    temp["Type"] = right[i]["Type"];
                }
                diff[name].append(temp);
            }
            found = true;
            break;
        }
        if (!found) {
            diff[name].append(right[i]);
        }
    }


}

void
json_compare_commands(Json::Value &base, Json::Value &big, Json::Value &diff, string name)
{
    Json::Value left = base[name];
    Json::Value right = big[name];

    if (left.type() != right.type() || left.empty()) {
        if (!right.empty()) {
            diff[name] = right;
        }
        return;
    }

    if (!left.isArray()) {
        cerr << name << " member not an array\n";
        return;
    }

    for (unsigned int i=0; i<right.size(); i++) {
        bool found = false;
        /* find matching entry in base */
        for (unsigned int j=0; j<left.size(); j++) {
            Json::Value temp = Json::Value::null;
            if (left[j]["Code"] != right[i]["Code"] ||
                left[j]["Flags"]["Must"] != right[i]["Flags"]["Must"]) {
                continue;
            }
            Json::Value::Members members = right[i].getMemberNames();
            for (auto it = members.begin() ; it != members.end(); ++it) {
                if (*it == "Flags") {
                    json_compare_objects(left[j], right[i], temp, *it);
                    continue;
                }
                if (left[j].isMember(*it)) {
                    if (left[j][*it] != right[i][*it]) {
                        temp[*it] = right[i][*it];
                    }
                } else {
                    temp[*it] = right[i][*it];
                }
            }
            if (!temp.isNull()) {
                temp["Code"] = right[i]["Code"];
                if (!right[i]["Flags"]["Must"].isNull()) {
                    temp["Flags"]["Must"] = right[i]["Flags"]["Must"];
                }
                diff[name].append(temp);
            }
            found = true;
            break;
        }
        if (!found) {
            diff[name].append(right[i]);
        }
    }


}

void
json_compare_rules(Json::Value &base, Json::Value &big, Json::Value &diff, string name)
{
    Json::Value left = base[name];
    Json::Value right = big[name];

    if (left.type() != right.type() || left.empty()) {
        if (!right.empty()) {
            diff[name] = right;
        }
        return;
    }

    if (!left.isArray()) {
        cerr << name << " member not an array\n";
        return;
    }

    for (unsigned int i=0; i<right.size(); i++) {
        bool found = false;
        /* find matching entry in base */
        for (unsigned int j=0; j<left.size(); j++) {
            Json::Value temp = Json::Value::null;
            if (name == "AVPRules") {
                if (left[j]["AVP"] != right[i]["AVP"]
                    || left[j]["Vendor"] != right[i]["Vendor"]) {
                    continue;
                }
            } else {
                if (left[j]["Command"] != right[i]["Command"]) {
                    continue;
                }
            }
            json_compare_arrays(left[j], right[i], temp, "Content", "AVP", "Vendor");
            if (!temp.isNull()) {
                if (name == "AVPRules") {
                    temp["AVP"] = right[i]["AVP"];
                    if (!right[i]["Vendor"].isNull()) {
                        temp["Vendor"] = right[i]["Vendor"];
                    }
                } else {
                    temp["Command"] = right[i]["Command"];
                    if (!right[i]["Flags"]["Must"].isNull()) {
                        temp["Flags"]["Must"] = right[i]["Flags"]["Must"];
                    }
                }
                diff[name].append(temp);
            }
            found = true;
            break;
        }
        if (!found) {
            diff[name].append(right[i]);
        }
    }
}

void
json_compare_arrays(Json::Value &base, Json::Value &big, Json::Value &diff, string name, string key, string key2)
{
    Json::Value left = base[name];
    Json::Value right = big[name];

    if (left.type() != right.type() || left.empty()) {
        if (!right.empty()) {
            diff[name] = right;
        }
        return;
    }

    if (!left.isArray()) {
        cerr << name << " member not an array\n";
        return;
    }

    for (unsigned int i=0; i<right.size(); i++) {
        bool found = false;
        /* find matching entry in base */
        for (unsigned int j=0; j<left.size(); j++) {
            Json::Value temp = Json::Value::null;
            if (left[j][key] != right[i][key]) {
                continue;
            }
            if (!key2.empty() && left[j][key2] != right[i][key2]) {
                continue;
            }
            Json::Value::Members members = right[i].getMemberNames();
            for (auto it = members.begin() ; it != members.end(); ++it) {
                if (left[j].isMember(*it)) {
                    if (left[j][*it] != right[i][*it]) {
                        temp[*it] = right[i][*it];
                    }
                } else {
                    temp[*it] = right[i][*it];
                }
            }
            if (!temp.isNull()) {
                temp[key] = right[i][key];
                if (key == "AVP" && !right[i]["Vendor"].isNull()) {
                    temp["Vendor"] = right[i]["Vendor"];
                }
                diff[name].append(temp);
            }
            found = true;
            break;
        }
        if (!found) {
            diff[name].append(right[i]);
        }
    }
}

void
compare_all(Json::Value &base, Json::Value &big, Json::Value &diff)
{
        std::string none;
        json_compare_arrays(base, big, diff, "Applications", "Code", none);
        json_compare_avps(base, big, diff, "AVPs");
        json_compare_rules(base, big, diff, "AVPRules");
        json_compare_commands(base, big, diff, "Commands");
        json_compare_rules(base, big, diff, "CommandRules");
        json_compare_arrays(base, big, diff, "Types", "Name", none);
        json_compare_arrays(base, big, diff, "Vendors", "Code", none);
}

int
main(int argc, char *argv[])
{
        Json::Value base = Json::Value::null;
        Json::Value big = Json::Value::null;
        Json::Value diff = Json::Value::null;
        Json::Value reverse_diff = Json::Value::null;
        Json::Reader reader;
        Json::StyledWriter writer;
        ifstream input;

        if (argc != 3) {
                cout << "usage: " << argv[0] << " small big" << endl; 
                cout << endl << "Prints added and changed entries that are in 'big' but not in 'small'," << endl << "or have changed between the two." << endl;
                exit(1);
        }

        input.open(argv[1]);
        if (!reader.parse(input, base)) {
                cerr << "error parsing JSON file '" << argv[1] << "'\n";
                exit(1);
        }
        input.close();

        input.open(argv[2]);
        if (!reader.parse(input, big)) {
                cerr << "error parsing JSON file '" << argv[2] << "'\n";
                exit(1);
        }
        input.close();

        compare_all(base, big, diff);
        if (diff != Json::Value::null) {
                std::string value_str = writer.write(diff);
                cout << value_str << endl;
        }

        compare_all(big, base, reverse_diff);
        if (reverse_diff != Json::Value::null) {
                cerr << argv[2] << " is missing entries that are in " << argv[1] << " -- please call " << argv[0] << " with swapped arguments" << endl;
        }

	return 0;
}
