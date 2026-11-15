/*
 * help.c -- WaveSmith's manual. See help.h. PrismaRec's (the mixer's and
 * Warden's before it), with WaveSmith's names.
 *
 * WHERE THE MANUAL IS: one per language, Help/<language>/WaveSmith.guide
 * beside the program, where the Installer puts them. Probed, the user's
 * language first, then English:
 *
 *   PROGDIR:Help/<lang>/   WaveSmith's own drawer
 *   Prisma:Help/<lang>/    the Prisma drawer, once WaveSmith is in the
 *                          driver package's
 *   HELP:<lang>/           the standard AmigaGuide layout
 *
 * then PROGDIR:WaveSmith.guide and HELP:WaveSmith.guide.
 *
 * WARNING: HELP: and Prisma: are assigns that may not exist, and a probe
 * on a missing volume would ask for it: pr_WindowPtr is -1 meanwhile.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <libraries/amigaguide.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/amigaguide.h>

#include "help.h"
#include "wavesmith_locale.h"	/* WS_LanguageName() */

#define HELP_APPNAME	"WaveSmith"
#define HELP_ASSIGN	"Prisma:"

#define DBG(x)

static void StrCopy(char *dst, CONST_STRPTR src, ULONG size)
{
    ULONG i;

    for (i = 0; i + 1 < size && src[i]; i++) dst[i] = (char)src[i];
    dst[i] = 0;
}

static void StrAppend(char *dst, CONST_STRPTR src, ULONG size)
{
    ULONG n;

    for (n = 0; n < size && dst[n]; n++)
        ;
    if (n < size) StrCopy(dst + n, src, size - n);
}

static void MemClear(APTR mem, ULONG size)
{
    UBYTE *p = (UBYTE *)mem;

    while (size--) *p++ = 0;
}

struct Library *AmigaGuideBase = NULL;

/* <here><language>/WaveSmith.guide, for the language asked about. */
static const char *const helpLangDirs[] =
{
    "PROGDIR:Help/",
    HELP_ASSIGN "Help/",
    "HELP:"
};
#define HELP_LANGDIRS (sizeof(helpLangDirs) / sizeof(helpLangDirs[0]))

/* And the last resort: a guide with no language drawer round it. */
static const char *const helpPaths[] =
{
    "PROGDIR:" HELP_APPNAME ".guide",
    "HELP:" HELP_APPNAME ".guide"
};
#define HELP_PATH_COUNT (sizeof(helpPaths) / sizeof(helpPaths[0]))

static BOOL Help_Readable(CONST_STRPTR path)
{
    BPTR lock;

    lock = Lock((STRPTR)path, ACCESS_READ);
    if (lock == 0)
    {
        return FALSE;
    }
    UnLock(lock);
    return TRUE;
}

/* <dir><lang>/WaveSmith.guide in each language drawer, into hc_Path. TRUE when
 * one is there. */
static BOOL Help_TryLanguage(struct HelpContext *help, CONST_STRPTR lang)
{
    ULONG i;

    if (lang == NULL || lang[0] == '\0')
    {
        return FALSE;
    }
    for (i = 0; i < HELP_LANGDIRS; i++)
    {
        StrCopy(help->hc_Path, (CONST_STRPTR)helpLangDirs[i],
                sizeof(help->hc_Path));
        StrAppend(help->hc_Path, lang, sizeof(help->hc_Path));
        StrAppend(help->hc_Path, (CONST_STRPTR)"/" HELP_APPNAME ".guide",
                  sizeof(help->hc_Path));
        DBG(("help: probe '%s'\n", (LONG)help->hc_Path));
        if (Help_Readable((CONST_STRPTR)help->hc_Path))
        {
            return TRUE;
        }
    }
    return FALSE;
}

/* Fills hc_Path with the first manual that is really there. */
static BOOL Help_FindFile(struct HelpContext *help)
{
    struct Process *proc;
    APTR  oldWindow;
    ULONG i;
    BOOL  found;

    proc = (struct Process *)FindTask(NULL);
    oldWindow = proc->pr_WindowPtr;
    proc->pr_WindowPtr = (APTR)-1L;      /* no requesters while probing */

    /* The user's own language first, then English. */
    found = Help_TryLanguage(help, WS_LanguageName());
    if (!found)
    {
        found = Help_TryLanguage(help, (CONST_STRPTR)"english");
    }
    for (i = 0; !found && i < HELP_PATH_COUNT; i++)
    {
        found = Help_Readable((CONST_STRPTR)helpPaths[i]);
        DBG(("help: probe '%s' -> %ld\n", (LONG)helpPaths[i], (LONG)found));
        if (found)
        {
            StrCopy(help->hc_Path, (CONST_STRPTR)helpPaths[i],
                    sizeof(help->hc_Path));
        }
    }

    proc->pr_WindowPtr = oldWindow;
    return found;
}

static BOOL Help_Ensure(struct HelpContext *help)
{
    if (help->hc_Handle != NULL)
    {
        return TRUE;
    }
    if (AmigaGuideBase == NULL)
    {
        AmigaGuideBase = OpenLibrary((STRPTR)"amigaguide.library", 39);
    }
    if (AmigaGuideBase == NULL)
    {
        DBG(("help: no amigaguide.library\n"));
        return FALSE;
    }
    if (!Help_FindFile(help))
    {
        DBG(("help: no manual installed\n"));
        return FALSE;
    }

    /* Everything but the name, the screen and the base name stays zero; the
     * node is chosen afterwards, because the async open ignores nag_Node. */
    MemClear(&help->hc_Nag, sizeof(help->hc_Nag));
    help->hc_Nag.nag_Name     = (STRPTR)help->hc_Path;
    help->hc_Nag.nag_Screen   = help->hc_Screen;
    help->hc_Nag.nag_BaseName = (STRPTR)HELP_APPNAME;
    help->hc_Nag.nag_Client   = NULL;    /* private: must be NULL */

    help->hc_Handle = OpenAmigaGuideAsync(&help->hc_Nag, TAG_END);
    if (help->hc_Handle == NULL)
    {
        DBG(("help: OpenAmigaGuideAsync failed (%ld)\n", (LONG)IoErr()));
        return FALSE;
    }
    DBG(("help: opened '%s'\n", (LONG)help->hc_Path));
    return TRUE;
}

/* WINDOWTOFRONT, then LINK. LINK alone shows the node but leaves the window
 * where it is: a second Help changed the page of a window behind the deck
 * (MEASURED in PrismaRec's HelpKeys, 15 Oct 2026). WINDOWTOFRONT after the
 * LINK lost the LINK -- every Help showed the first page -- so it goes
 * first: amigaguide.library does it at once, the LINK lays the node out
 * afterwards. */
static void Help_SendLink(struct HelpContext *help, CONST_STRPTR node)
{
    char cmd[HELP_NODE_MAX + 8];

    StrCopy(cmd, (CONST_STRPTR)"LINK ", sizeof(cmd));
    StrAppend(cmd, node, sizeof(cmd));
    DBG(("help: %s\n", (LONG)cmd));
    SendAmigaGuideCmd(help->hc_Handle, (STRPTR)"WINDOWTOFRONT", TAG_END);
    SendAmigaGuideCmd(help->hc_Handle, (STRPTR)cmd, TAG_END);
}

BOOL Help_Show(struct HelpContext *help, CONST_STRPTR node)
{
    if (help == NULL || !Help_Ensure(help))
    {
        return FALSE;
    }
    if (node == NULL || node[0] == '\0')
    {
        node = (CONST_STRPTR)"Main";
    }
    if (help->hc_Ready)
    {
        Help_SendLink(help, node);
    }
    else
    {
        /* The client is not up yet and would drop this one silently: the
         * node is kept, and Help_HandleMessages sends it when it may. */
        StrCopy(help->hc_Pending, node, sizeof(help->hc_Pending));
    }
    return TRUE;
}

ULONG Help_Signal(struct HelpContext *help)
{
    if (help == NULL || help->hc_Handle == NULL)
    {
        return 0;
    }
    return AmigaGuideSignal(help->hc_Handle);
}

void Help_HandleMessages(struct HelpContext *help)
{
    struct AmigaGuideMsg *agm;
    BOOL failed;

    if (help == NULL || help->hc_Handle == NULL)
    {
        return;
    }
    /* Every message is replied, whatever it is, or the guide process stops.
     * Only one is acted on: ActiveToolID/ToolStatusID says the client has
     * read the NewAmigaGuide and will honour commands from now on -- unless
     * ToolStatusID carries an error in agm_Pri_Ret. That one used to count as
     * ready too, and every later Help went to a client that had failed,
     * silently (code review, 5 Oct 2026). Now the context is closed, and the
     * next Help starts again and says so if it fails again. */
    failed = FALSE;
    while ((agm = GetAmigaGuideMsg(help->hc_Handle)) != NULL)
    {
        if (agm->agm_Type == ToolStatusID && agm->agm_Pri_Ret != 0)
        {
            failed = TRUE;
        }
        else if (agm->agm_Type == ActiveToolID ||
                 agm->agm_Type == ToolStatusID)
        {
            help->hc_Ready = TRUE;
        }
        ReplyAmigaGuideMsg(agm);
    }
    if (failed)
    {
        DBG(("help: the client failed to start\n"));
        Help_Close(help);
        return;
    }
    if (help->hc_Ready && help->hc_Pending[0] != '\0')
    {
        Help_SendLink(help, (CONST_STRPTR)help->hc_Pending);
        help->hc_Pending[0] = '\0';
    }
}

void Help_Close(struct HelpContext *help)
{
    if (help == NULL || help->hc_Handle == NULL)
    {
        return;
    }
    /* This closes the client's windows too, which may be on the screen the
     * editor is on -- so it runs before that screen goes. */
    CloseAmigaGuide(help->hc_Handle);
    help->hc_Handle = NULL;
    help->hc_Ready = FALSE;             /* a re-open handshakes again */
    help->hc_Pending[0] = '\0';
}

void Help_Cleanup(struct HelpContext *help)
{
    Help_Close(help);
    if (AmigaGuideBase != NULL)
    {
        CloseLibrary(AmigaGuideBase);
        AmigaGuideBase = NULL;
    }
}
