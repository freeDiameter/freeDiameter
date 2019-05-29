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

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <json/SchemaValidator.h>

[[noreturn]]
void usage(char *prg, int exit_status) {
    FILE *f = (exit_status ? stderr : stdout);

    fprintf(f, "Usage: %s [-hV] [-i include] [-N variable-name] [-n namespace] [-t type] json [output]\n", prg);
    fprintf(f, "options:\n\
  -h, --help              print this usage message and exit\n\
  -i, --include FILE      include FILE in output C source file\n\
  -n, --namespace NAME    specify namespace in which to define variable\n\
  -N, --name NAME         specify name of variable to define\n\
  --no-validate           don't validate against JSON meta schema (default)\n\
  --type TYPE             specify type of variable (char * (default) or std::string)\n\
  --validate              validate against JSON meta schema\n\
  --validate-only         validate against JSON meta schema and exit (don't create C source)\n\
");

    exit(exit_status);
}

const char *OPTIONS = "hi:N:n:t:V";

enum {
    OPT_NO_VALIDATE = 256,
    OPT_VALIDATE_ONLY
};
struct option options[] = {
    { "help", no_argument, NULL, 'h' },
    { "include", required_argument, NULL, 'i' },
    { "name", required_argument, NULL, 'N' },
    { "namespace", required_argument, NULL, 'n' },
    { "no-validate", no_argument, NULL, OPT_NO_VALIDATE },
    { "type", required_argument, NULL, 't' },
    { "validate", no_argument, NULL, 'V' },
    { "validate-only", no_argument, NULL, OPT_VALIDATE_ONLY },
    { NULL, 0, NULL, 0 }
};

char *read_full_file (const char *filename, off_t max_size, off_t* size_ret, const char *desc) {
    FILE *fp;
    if ((fp = fopen (filename, "rb")) == NULL) {
        fprintf (stderr, "couldn't open %s file [%s]: %d, %s\n", desc, filename, errno, strerror(errno));
        return NULL;
    }
    struct stat stat_buf;
    if (fstat (fileno(fp), &stat_buf) < 0) {
        fprintf (stderr, "couldn't stat %s file [%s]: %d, %s\n", desc, filename, errno, strerror(errno));
        fclose (fp);
        return NULL;
    }
    off_t n = stat_buf.st_size;
    if (max_size > 0 && n > max_size) {
        fprintf (stderr, "%s file [%s] is larger than %lld bytes\n", desc, filename, (long long)max_size);
        fclose (fp);
        return NULL;
    }
    char *buf;
    if ((buf = (char *) malloc ((size_t)n+1)) == NULL) {
        fprintf (stderr, "error allocating %lld bytes for read of %s file [%s]\n", (long long)n, desc, filename);
        fclose (fp);
        return NULL;
    }
    if (fread (buf, 1, (size_t)n, fp) < (size_t) n) {
        fprintf (stderr, "error reading %s file [%s]: %d, %s\n", desc, filename, errno, strerror(errno));
        fclose (fp);
        free (buf);
        return NULL;
    }

    fclose (fp);
    buf[n] = '\0';
    if (size_ret != NULL)
        *size_ret = n;
    return buf;
}

int main (int argc, char **argv) {
    char *name_space = NULL;
    char *name = NULL;
    bool free_name = false;
    char default_type[] = "char *";
    char *type = default_type;
    bool validate = false;
    bool convert = true;
    char *include = NULL;

    int c;
    while ((c=getopt_long(argc, argv, OPTIONS, options, NULL)) != EOF) {
        switch (c) {
            case 'i':
                include = optarg;
                break;

            case 'N':
                name = optarg;
                break;

            case 'n':
                name_space = optarg;
                break;

            case 't':
                type = optarg;
                break;

            case 'V':
                validate = true;
                break;

            case OPT_NO_VALIDATE:
                validate = false;
                break;

            case OPT_VALIDATE_ONLY:
                validate = true;
                convert = false;
                break;

            case 'h':
                usage(argv[0], 0);

            default:
                usage(argv[0], 1);
        }
    }

    if (optind != argc - 2 && optind != argc - 1)
        usage(argv[0], 1);

    char *input = argv[optind];
    char *output = NULL;
    if (optind == argc -2) {
        output = argv[optind+1];
    }

    char *str = read_full_file(input, 10*1024*1024, NULL, input);

    Json::Reader reader;

    Json::Value json;
    if (!reader.parse(str, json)) {
	fprintf(stderr, "%s: parse error: %s\n", input, reader.getFormattedErrorMessages().c_str());
        exit(1);
    }

    if (validate) {
        std::string error_message;
        Json::SchemaValidator *validator;

        try {
            validator = Json::SchemaValidator::create_meta_validator();
        }
        catch (Json::SchemaValidator::Exception e) {
            fprintf(stderr, "%s: can't create meta schema validator\n", argv[0]);
            exit(1);
        }

        if (!validator->validate(json)) {
            const std::vector<Json::SchemaValidator::Error> errors = validator->errors();

            for (unsigned int i=0; i<errors.size(); i++)
                fprintf(stderr, "%s:%s: %s\n", input, errors[i].path.c_str(), errors[i].message.c_str());
            exit(1);
        }

        try {
            Json::SchemaValidator v(json);
        }
        catch (Json::SchemaValidator::Exception e) {
            fprintf(stderr, "%s: can't create schema validator: %s\n", argv[0], e.type_message().c_str());
            for (auto error : e.errors) {
                fprintf(stderr, "%s:%s: %s\n", input, error.path.c_str(), error.message.c_str());
            }
            exit(1);
        }
    }

    if (!convert) {
        exit(0);
    }

    FILE *fin = fopen(input, "r");
    if (fin == NULL) {
        fprintf(stderr, "%s: can't open schema file [%s]: %s\n", argv[0], input, strerror(errno));
        exit(1);
    }

    FILE *fout;

    if (output) {
        fout = fopen(output, "w");
        if (fout == NULL) {
            fprintf(stderr, "%s: can't create output file [%s]: %s\n", argv[0], output, strerror(errno));
            exit(1);
        }
    }
    else {
        fout = stdout;
    }

    if (name == NULL) {
        char *base = basename(input);
        char *end = strrchr(base, '.');
        if (end == NULL) {
            end = base + strlen(base);
        }

        name = strndup(base, static_cast<size_t>(end-base));

        for (char *p = name; *p; p++) {
            if ((*p >= 'A' && *p < 'Z') || (*p >= 'a' && *p < 'z') || *p == '_') {
                continue;
            }
            else if (*p >= '0' && *p <= '9') {
                if (p > name) {
                    continue;
                }
            }
            *p = '_';
        }
    }
    else {
        for (const char *p = name; *p; p++) {
            if ((*p >= 'A' && *p < 'Z') || (*p >= 'a' && *p < 'z') || *p == '_') {
                continue;
            }
            else if (*p >= '0' && *p <= '9') {
                if (p > name) {
                    continue;
                }
            }
            else if (p[0] == ':' && p[1] == ':') {
                p += 1;
                continue;
            }

            fprintf(stderr, "%s: name [%s] is not a valid C identifier\n", argv[0], name);
            exit(1);
        }
    }

    if (include) {
        fprintf(fout, "#include <%s>\n\n", include);
    }

    if (strcmp(type, "std::string") == 0) {
        fputs("#include <string>\n\n", fout);
    }

    if (name_space) {
        fprintf(fout, "namespace %s {\n\n", name_space);
    }

    fprintf(fout, "const %s %s = \"\\\n", type, name);

    if (free_name) {
        free(name);
        name = NULL;
    }

    char line[8192];

    while (fgets(line, sizeof(line), fin)) {
        if (line[strlen(line)-1] == '\n')
            line[strlen(line)-1] = '\0';

        char *p = line;
        char *end = line+strlen(line);
        char *q;
        do {
            q = p + strcspn(p, "\"\\");
            if (q < end) {
                fprintf(fout, "%.*s\\%c", (int)(q-p), p, *q);
                p=q+1;
            }
            else
                fprintf(fout, "%s", p);
        } while (q < end);

        fputs(" \\n\\\n", fout);
    }

    fputs("\";\n", fout);

    if (name_space) {
        fputs("\n}\n", fout);
    }

    if (ferror(fin)) {
        fprintf(stderr, "%s: read error on schema file [%s]: %s\n", argv[0], input, strerror(errno));
        fclose(fout);
        unlink(output);
        exit(1);
    }

    fclose(fin);

    if (ferror(fout)) {
        fprintf(stderr, "%s: write error on output file [%s]: %s\n", argv[0], output, strerror(errno));
        fclose(fout);
        unlink(output);
        exit(1);
    }

    fclose(fout);

    exit(0);
}
