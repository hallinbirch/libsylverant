/*
    This file is part of Sylverant PSO Server.

    Copyright (C) 2009, 2010, 2011 Lawrence Sebald

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License version 3
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "sylverant/config.h"
#include "sylverant/debug.h"

#ifndef LIBXML_TREE_ENABLED
#error You must have libxml2 with tree support built-in.
#endif

#define XC (const xmlChar *)

static int handle_shipgate(xmlNode *n, sylverant_shipcfg_t *cfg) {
    xmlChar *ip, *port;
    int rv;
    unsigned long rv2;

    /* Grab the attributes of the tag. */
    ip = xmlGetProp(n, XC"ip");
    port = xmlGetProp(n, XC"port");

    /* Make sure we have both of them... */
    if(!ip || !port) {
        debug(DBG_ERROR, "IP or port not given for shipgate\n");
        rv = -1;
        goto err;
    }

    /* Parse the IP address out */
    rv = inet_pton(AF_INET, (char *)ip, &cfg->shipgate_ip);

    if(rv < 1) {
        debug(DBG_ERROR, "Invalid IP address given for shipgate: %s\n",
              (char *)ip);
        rv = -2;
        goto err;
    }

    /* Parse the port out */
    rv2 = strtoul((char *)port, NULL, 0);

    if(rv2 == 0 || rv2 > 0xFFFF) {
        debug(DBG_ERROR, "Invalid port given for shipgate: %s\n", (char *)port);
        rv = -3;
        goto err;
    }

    cfg->shipgate_port = (uint16_t)rv2;
    rv = 0;

err:
    xmlFree(ip);
    xmlFree(port);
    return rv;
}

static int handle_net(xmlNode *n, sylverant_ship_t *cur) {
    xmlChar *ip, *port;
    int rv;
    unsigned long rv2;

    /* Grab the attributes of the tag. */
    ip = xmlGetProp(n, XC"ip");
    port = xmlGetProp(n, XC"port");

    /* Make sure we have both of them... */
    if(!ip || !port) {
        debug(DBG_ERROR, "IP or port not given for ship\n");
        rv = -1;
        goto err;
    }

    /* Parse the IP address out */
    rv = inet_pton(AF_INET, (char *)ip, &cur->ship_ip);

    if(rv < 1) {
        debug(DBG_ERROR, "Invalid IP address given for ship: %s\n",
              (char *)ip);
        rv = -2;
        goto err;
    }

    /* Parse the port out */
    rv2 = strtoul((char *)port, NULL, 0);

    if(rv2 == 0 || rv2 > 0xFFFF) {
        debug(DBG_ERROR, "Invalid port given for ship: %s\n", (char *)port);
        rv = -3;
        goto err;
    }

    cur->base_port = (uint16_t)rv2;
    rv = 0;

err:
    xmlFree(ip);
    xmlFree(port);
    return rv;
}

static int handle_event(xmlNode *n, sylverant_ship_t *cur) {
    xmlChar *game, *lobby;
    int rv;
    long rv2;

    /* Grab the attributes of the tag. */
    game = xmlGetProp(n, XC"game");
    lobby = xmlGetProp(n, XC"lobby");

    /* Make sure we have the data */
    if(!game || !lobby) {
        debug(DBG_ERROR, "Event number not given\n");
        rv = -1;
        goto err;
    }

    /* Parse the game event out */
    errno = 0;
    rv2 = strtol((char *)game, NULL, 0);

    if(errno || rv2 > 6) {
        debug(DBG_ERROR, "Invalid game event given for ship: %s\n",
              (char *)game);
        rv = -3;
        goto err;
    }

    cur->game_event = (int)rv2;

    /* Parse the lobby event out */
    rv2 = strtol((char *)lobby, NULL, 0);

    if(errno || rv2 > 14) {
        debug(DBG_ERROR, "Invalid lobby event given for ship: %s\n",
              (char *)lobby);
        rv = -3;
        goto err;
    }

    cur->lobby_event = (int)rv2;
    rv = 0;

err:
    xmlFree(game);
    xmlFree(lobby);
    return rv;
}

static int handle_info(xmlNode *n, sylverant_ship_t *cur) {
    xmlChar *fn, *desc;
    int count;
    void *tmp;
    int rv = 0;

    /* Grab the attributes of the tag. */
    fn = xmlGetProp(n, XC"file");
    desc = xmlGetProp(n, XC"desc");

    /* Make sure we have both of them... */
    if(!fn || !desc) {
        debug(DBG_ERROR, "Filename or description not given for info\n");
        rv = -1;
        goto err;
    }

    /* Allocate space for the new entry */
    count = cur->info_file_count;
    tmp = realloc(cur->info_files, (count + 1) * sizeof(char *));
    if(!tmp) {
        debug(DBG_ERROR, "Couldn't allocate space for info file\n");
        perror("realloc");
        rv = -2;
        goto err;
    }

    cur->info_files = (char **)tmp;

    /* Allocate space for the new description. */
    tmp = realloc(cur->info_files_desc, (count + 1) * sizeof(char *));
    if(!tmp) {
        debug(DBG_ERROR, "Couldn't allocate space for info desc\n");
        perror("realloc");
        rv = -3;
        goto err;
    }

    cur->info_files_desc = (char **)tmp;

    /* Copy the data in */
    cur->info_files[count] = fn;
    cur->info_files_desc[count] = desc;
    ++cur->info_file_count;

    return 0;

err:
    xmlFree(fn);
    xmlFree(desc);
    return rv;
}

static int handle_quests(xmlNode *n, sylverant_ship_t *cur) {
    xmlChar *fn;

    /* Grab the directory, if given */
    if((fn = xmlGetProp(n, XC"dir"))) {
        cur->quests_dir = (char *)fn;
        return 0;
    }

    /* If not, see if we have the file attribute */
    if((fn = xmlGetProp(n, XC"file"))) {
        cur->quests_file = (char *)fn;
        return 0;
    }

    /* If we don't have either, report the error */
    debug(DBG_ERROR, "Malformed quest tag, no file or dir given\n");
    return -1;
}

static int handle_limits(xmlNode *n, sylverant_ship_t *cur) {
    xmlChar *fn;

    /* Grab the attributes of the tag. */
    fn = xmlGetProp(n, XC"file");

    /* Make sure we have the data */
    if(!fn) {
        debug(DBG_ERROR, "Limits filename not given\n");
        return -1;
    }

    /* Copy it over to the struct */
    strncpy(cur->limits_file, (char *)fn, 255);
    cur->limits_file[255] = '\0';

    xmlFree(fn);
    return 0;
}

static int handle_motd(xmlNode *n, sylverant_ship_t *cur) {
    xmlChar *fn;

    /* Grab the attributes of the tag. */
    fn = xmlGetProp(n, XC"file");

    /* Make sure we have the data */
    if(!fn) {
        debug(DBG_ERROR, "MOTD filename not given\n");
        return -1;
    }

    /* Copy it over to the struct */
    strncpy(cur->motd_file, (char *)fn, 255);
    cur->motd_file[255] = '\0';

    xmlFree(fn);
    return 0;
}

static int handle_bans(xmlNode *n, sylverant_ship_t *cur) {
    xmlChar *fn;

    /* Grab the attributes of the tag. */
    fn = xmlGetProp(n, XC"file");

    /* Make sure we have the data */
    if(!fn) {
        debug(DBG_ERROR, "Bans filename not given\n");
        return -1;
    }

    /* Copy it over to the struct */
    cur->bans_file = (char *)fn;

    return 0;
}

static int handle_versions(xmlNode *n, sylverant_ship_t *cur) {
    xmlChar *v1, *v2, *pc, *gc, *ep3;
    int rv = 0;

    /* Grab the attributes of the tag. */
    v1 = xmlGetProp(n, XC"v1");
    v2 = xmlGetProp(n, XC"v2");
    pc = xmlGetProp(n, XC"pc");
    gc = xmlGetProp(n, XC"gc");
    ep3 = xmlGetProp(n, XC"ep3");

    /* Make sure we have the data */
    if(!v1 || !v2 || !pc || !gc || !ep3) {
        debug(DBG_ERROR, "Missing version\n");
        rv = -1;
        goto err;
    }

    /* Parse everything out */
    if(!xmlStrcmp(v1, XC"false")) {
        cur->shipgate_flags |= SHIPGATE_FLAG_NOV1;
    }

    if(!xmlStrcmp(v2, XC"false")) {
        cur->shipgate_flags |= SHIPGATE_FLAG_NOV2;
    }

    if(!xmlStrcmp(pc, XC"false")) {
        cur->shipgate_flags |= SHIPGATE_FLAG_NOPC;
    }

    if(!xmlStrcmp(gc, XC"false")) {
        cur->shipgate_flags |= SHIPGATE_FLAG_NOEP12;
    }

    if(!xmlStrcmp(ep3, XC"false")) {
        cur->shipgate_flags |= SHIPGATE_FLAG_NOEP3;
    }

err:    
    xmlFree(v1);
    xmlFree(v2);
    xmlFree(pc);
    xmlFree(gc);
    xmlFree(ep3);
    return rv;
}

static int handle_ship(xmlNode *n, sylverant_shipcfg_t **cfgp) {
    void *tmp;
    sylverant_ship_t *cur;
    sylverant_shipcfg_t *cfg = *cfgp;
    xmlChar *name, *blocks, *key, *gms, *menu, *gmonly;
    int rv;
    unsigned long rv2;
    xmlNode *n2;

    /* Allocate space for the new ship... */
    tmp = realloc(cfg, sizeof(sylverant_shipcfg_t) +
                  ((cfg->ship_count + 1) * sizeof(sylverant_ship_t)));

    if(!tmp) {
        debug(DBG_ERROR, "Couldn't allocate space for ship\n");
        perror("realloc");
        return -1;
    }

    *cfgp = cfg = (sylverant_shipcfg_t *)tmp;
    cur = &cfg->ships[cfg->ship_count++];

    /* Clear the new ship */
    memset(cur, 0, sizeof(sylverant_ship_t));

    /* Grab the attributes of the <ship> tag. */
    name = xmlGetProp(n, XC"name");
    blocks = xmlGetProp(n, XC"blocks");
    key = xmlGetProp(n, XC"key");
    gms = xmlGetProp(n, XC"gms");
    menu = xmlGetProp(n, XC"menu");
    gmonly = xmlGetProp(n, XC"gmonly");

    if(!name || !blocks || !key || !gms || !gmonly || !menu) {
        debug(DBG_ERROR, "Required attribute of ship not found\n");
        rv = -1;
        goto err;
    }

    /* Copy out the strings out that we need */
    strncpy(cur->name, (char *)name, 255);
    strncpy(cur->key_file, (char *)key, 255);
    strncpy(cur->gm_file, (char *)gms, 255);
    
    cur->name[255] = '\0';
    cur->key_file[255] = '\0';
    cur->gm_file[255] = '\0';

    /* Copy out the gmonly flag */
    if(!xmlStrcmp(gmonly, XC"true")) {
        cur->shipgate_flags |= SHIPGATE_FLAG_GMONLY;
    }

    /* Grab the menu code */
    rv = xmlStrlen(menu);

    if(rv == 2 && isalpha(menu[0]) && isalpha(menu[1])) {
        cur->menu_code = ((uint16_t)menu[0]) | (((uint16_t)(menu[1])) << 8);
    }
    else if(rv) {
        debug(DBG_ERROR, "Invalid menu code given");
        rv = -2;
        goto err;
    }

    /* Copy out the number of blocks */
    rv2 = strtoul((char *)blocks, NULL, 0);

    if(rv2 == 0 || rv2 > 16) {
        debug(DBG_ERROR, "Invalid block count given: %s\n", (char *)blocks);
        rv = -3;
        goto err;
    }
    
    cur->blocks = (int)rv2;

    /* Parse out the children of the <ship> tag. */
    n2 = n->children;
    while(n2) {
        if(n2->type != XML_ELEMENT_NODE) {
            /* Ignore non-elements. */
            n2 = n2->next;
            continue;
        }
        else if(!xmlStrcmp(n2->name, XC"net")) {
            if(handle_net(n2, cur)) {
                rv = -3;
                goto err;
            }
        }
        else if(!xmlStrcmp(n2->name, XC"event")) {
            if(handle_event(n2, cur)) {
                rv = -4;
                goto err;
            }
        }
        else if(!xmlStrcmp(n2->name, XC"info")) {
            if(handle_info(n2, cur)) {
                rv = -5;
                goto err;
            }
        }
        else if(!xmlStrcmp(n2->name, XC"quests")) {
            if(handle_quests(n2, cur)) {
                rv = -6;
                goto err;
            }
        }
        else if(!xmlStrcmp(n2->name, XC"limits")) {
            if(handle_limits(n2, cur)) {
                rv = -7;
                goto err;
            }
        }
        else if(!xmlStrcmp(n2->name, XC"motd")) {
            if(handle_motd(n2, cur)) {
                rv = -8;
                goto err;
            }
        }
        else if(!xmlStrcmp(n2->name, XC"versions")) {
            if(handle_versions(n2, cur)) {
                rv = -9;
                goto err;
            }
        }
        else if(!xmlStrcmp(n2->name, XC"bans")) {
            if(handle_bans(n2, cur)) {
                rv = -10;
                goto err;
            }
        }
        else {
            debug(DBG_WARN, "Invalid Tag %s on line %hu\n", (char *)n2->name,
                  n2->line);
        }

        n2 = n2->next;
    }

    rv = 0;

err:
    xmlFree(name);
    xmlFree(blocks);
    xmlFree(key);
    xmlFree(gms);
    xmlFree(gmonly);
    xmlFree(menu);
    return rv;
}

int sylverant_read_ship_config(const char *f, sylverant_shipcfg_t **cfg) {
    xmlParserCtxtPtr cxt;
    xmlDoc *doc;
    xmlNode *n;
    sylverant_shipcfg_t *rv;
    int irv = 0;

    /* Allocate space for the base of the config. */
    rv = (sylverant_shipcfg_t *)malloc(sizeof(sylverant_shipcfg_t));

    if(!rv) {
        *cfg = NULL;
        debug(DBG_ERROR, "Couldn't allocate space for ship config\n");
        perror("malloc");
        return -1;
    }

    /* Clear out the config. */
    memset(rv, 0, sizeof(sylverant_shipcfg_t));

    /* Create an XML Parsing context */
    cxt = xmlNewParserCtxt();
    if(!cxt) {
        debug(DBG_ERROR, "Couldn't create parsing context for ship config\n");
        irv = -2;
        goto err;
    }

    /* Open the configuration file for reading. */
    if(f) {
        doc = xmlReadFile(f, NULL, XML_PARSE_DTDVALID);
    }
    else {
        doc = xmlReadFile(sylverant_ship_cfg, NULL, XML_PARSE_DTDVALID);
    }

    if(!doc) {
        xmlParserError(cxt, "Error in parsing ship config");
        irv = -3;
        goto err_cxt;
    }

    /* Make sure the document validated properly. */
    if(!cxt->valid) {
        xmlParserValidityError(cxt, "Validity Error parsing ship config");
        irv = -4;
        goto err_doc;
    }

    /* If we've gotten this far, we have a valid document, now go through and
       add in entries for everything... */
    n = xmlDocGetRootElement(doc);

    if(!n) {
        debug(DBG_WARN, "Empty ship config document\n");
        irv = -5;
        goto err_doc;
    }

    /* Make sure the config looks sane. */
    if(xmlStrcmp(n->name, XC"ships")) {
        debug(DBG_WARN, "Ship config does not appear to be the right type\n");
        irv = -6;
        goto err_doc;
    }

    n = n->children;
    while(n) {
        if(n->type != XML_ELEMENT_NODE) {
            /* Ignore non-elements. */
            n = n->next;
            continue;
        }
        else if(!xmlStrcmp(n->name, XC"shipgate")) {
            if(handle_shipgate(n, rv)) {
                irv = -7;
                goto err_doc;
            }
        }
        else if(!xmlStrcmp(n->name, XC"ship")) {
            if(handle_ship(n, &rv)) {
                irv = -8;
                goto err_doc;
            }
        }
        else {
            debug(DBG_WARN, "Invalid Tag %s on line %hu\n", (char *)n->name,
                  n->line);
        }

        n = n->next;
    }

    *cfg = rv;

    /* Cleanup/error handling below... */
err_doc:
    xmlFreeDoc(doc);
err_cxt:
    xmlFreeParserCtxt(cxt);

err:
    if(irv && irv > -7) {
        free(rv);
        *cfg = NULL;
    }
    else if(irv) {
        sylverant_free_ship_config(rv);
        *cfg = NULL;
    }

    return irv;
}

int sylverant_free_ship_config(sylverant_shipcfg_t *cfg) {
    int i, j;

    /* Make sure we actually have a valid configuration pointer. */
    if(cfg) {
        /* Look through each ship to clean up its info files. */
        for(i = 0; i < cfg->ship_count; ++i) {
            if(cfg->ships[i].info_files) {
                for(j = 0; j < cfg->ships[i].info_file_count; ++j) {
                    free(cfg->ships[i].info_files[j]);
                    free(cfg->ships[i].info_files_desc[j]);
                }

                free(cfg->ships[i].info_files);
                free(cfg->ships[i].info_files_desc);
            }

            free(cfg->ships[i].quests_file);
            xmlFree(cfg->ships[i].bans_file);
        }

        /* Clean up the base structure. */
        free(cfg);
    }
}
