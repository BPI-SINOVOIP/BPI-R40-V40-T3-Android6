/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : cdx_plugin.c
 * Description : cdx_plugin
 * History :
 *
 */

#include <log.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <CdxList.h>
#include <CdxTypes.h>

#include <vdecoder.h>
#include <cdx_plugin.h>

#include "iniparserapi.h"

typedef struct CdxPluginListS
{
    CdxListT list;
    int size;
} CdxPluginListS;

typedef struct CdxPluginNodeS
{
    CdxListNodeT node;
    cdx_char key[32];
    const cdx_char *id;
    const cdx_char *comment;
    const cdx_char *lib;
    const cdx_char *init;
    const cdx_char *exit;
    const cdx_char *reference;
    int             isload;
} CdxPluginNodeS;

struct CdxPluginListS g_pluging_list;
typedef void PluginInitFunc(void);

void CdxPluginInit(void)  __attribute__((constructor));

/*check plugin if already loaded*/
int PluginExist(CdxPluginNodeS *plugin)
{
    int ret = 0;
    if (plugin == NULL || plugin->id == NULL)
        return 0;
    if (plugin->isload == 1)
        ret = 1;
    return ret;
}

/*use id to get a plugin form g_pluging_list */
CdxPluginNodeS* GetPluginById(const char *id)
{
    struct CdxPluginNodeS *pluginNode = NULL;
    CdxListForEachEntry(pluginNode, &g_pluging_list.list, node)
    {
        if (strcasecmp(pluginNode->id, id) == 0)
        {
            return pluginNode;
        }
    }
    return NULL;
}

/*use key to get a plugin form g_pluging_list */
CdxPluginNodeS* GetPluginByKey(const char *key)
{
    struct CdxPluginNodeS *pluginNode = NULL;
    CdxListForEachEntry(pluginNode, &g_pluging_list.list, node)
    {
        if (strcasecmp(pluginNode->key, key) == 0)
        {
            return pluginNode;
        }
    }
    return NULL;
}

int DlOpenPlugin(CdxPluginNodeS *plugin)
{
    void *libFd = NULL;

    if (plugin == NULL)
        return 0;
    if (plugin->lib == NULL && strcasecmp(plugin->init, "") != 0)
        return 0;

    if(plugin->comment != NULL)
        logd("plugin %s comment is \"%s\"",plugin->id,plugin->comment);
    logd("plugin open lib: %s",plugin->lib);
    libFd = dlopen(plugin->lib, RTLD_NOW);
    PluginInitFunc *PluginInit = NULL;

    if (libFd == NULL)
    {
        loge("dlopen '%s' fail: %s", plugin->lib, dlerror());
        return 0;
    }

    if (plugin->init != NULL && strcasecmp(plugin->init, "") != 0)
    {
        PluginInit = dlsym(libFd, plugin->init);
        if (PluginInit == NULL)
        {
            logw("Invalid plugin,function %s not found.",plugin->init);
            return 1;
        }
        logd("plugin init : %s", plugin->init);
        PluginInit(); /* call init plugin */
    }

    return 1;
}

/*use id to load plugin into Cedarx*/
int LoadPlugin(const char *id)
{
    if (id == NULL)
        return 0;

    CdxPluginNodeS *plugin = NULL;
    int ret = 0;
    plugin = GetPluginById(id);

    if (plugin == NULL || plugin->id == NULL)
        return 0;
    if (PluginExist(plugin) == 1)
    {
        return 1;
    }

    //do load plugin reference
    if (plugin->reference != NULL && strcasecmp(plugin->reference, "") != 0 )
    {
        ret = LoadPlugin(plugin->reference);
        if (ret != 1)
        {
            loge("load plugin %s fail,because load it's reference %s fail",
                 plugin->id,plugin->reference);
            return 0;
        }
    }

    ret = DlOpenPlugin(plugin);
    return ret;
}

/*create a plugin entry when plugin key is found in config file*/
CdxPluginNodeS* ReadPluginEntry(const char *entry)
{
    logd("need to read plugin entry %s",entry);
    CdxPluginNodeS *pluginNode = NULL;
    int ret = 0;
    char key_id[128] = "";
    char key_comment[128] = "";
    char key_lib[128] = "";
    char key_init[128] = "";
    char key_exit[128] = "";
    char key_reference[128] = "";

    sprintf(key_id,"%s:id",entry);
    sprintf(key_comment,"%s:comment",entry);
    sprintf(key_lib,"%s:lib",entry);
    sprintf(key_init,"%s:init",entry);
    sprintf(key_exit,"%s:exit",entry);
    sprintf(key_reference,"%s:reference",entry);

    ret = IniParserFindEntry(entry);
    if (ret == 1)
    {
        pluginNode = malloc(sizeof(*pluginNode));
        memset(pluginNode, 0x00, sizeof(*pluginNode));

        sprintf(pluginNode->key,"%s",entry);

        pluginNode->id = IniParserGetString(key_id,NULL);
        pluginNode->comment = IniParserGetString(key_comment,NULL);
        pluginNode->lib = IniParserGetString(key_lib,NULL);
        pluginNode->init = IniParserGetString(key_init,NULL);
        pluginNode->exit = IniParserGetString(key_exit,NULL);
        pluginNode->reference = IniParserGetString(key_reference,"");
        pluginNode->isload = 0;
    }
    else
    {
        logd("read plugin entry %s fail!",entry);
    }

    return pluginNode;
}

void CdxPluginInit(void)
{
    logd("Plugin init!");
    AddVDPlugin();
    static int has_init = 0;
    int index = 0;
    int ret = -1;
    char plugin_entry[64]="";
    CdxPluginNodeS* pluginNode = NULL;

    CdxListInit(&g_pluging_list.list);
    g_pluging_list.size = 0;

    if (!has_init)
    {

        while (1)
        {
            sprintf(plugin_entry,"plugin-%d",index);
            pluginNode = ReadPluginEntry(plugin_entry);

            if (pluginNode == NULL)
                break;

            CdxListAddTail(&pluginNode->node, &g_pluging_list.list);
            g_pluging_list.size++;
            index++;
        }

        logd("have config %d plugin entry",g_pluging_list.size);
        logd("start to open plugin");

        CdxListForEachEntry(pluginNode, &g_pluging_list.list, node)
        {
            if (pluginNode->isload != 1)
            {
                ret = LoadPlugin(pluginNode->id);
                if (ret != 1)
                {
                    logd("load plugin id %s fail!",pluginNode->id);
                }
            }
        }

        has_init = 1;
    }
    return ;
}
