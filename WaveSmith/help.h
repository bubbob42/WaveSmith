/****** help.h/--background-- ***********************************************
*
*   NAME
*       help.h -- WaveSmith's manual, from the Help key and the menu.
*       (PrismaRec's help.h, the mixer's before it: the contract unchanged.)
*
*   FUNCTION
*       Asynchronous AmigaGuide help, from the published amigaguide.library
*       autodocs. The library is opened on the FIRST request and the client
*       is then kept for the rest of the run: its window comes and goes as
*       the reader browses or closes it, and a second request re-links to
*       the wanted node, which brings the window back to the front.
*
*       The whole feature is optional. Without amigaguide.library, or with
*       no manual installed, Help_Show returns FALSE and the caller says so
*       -- it is never an error.
*
*   NOTES
*       THE ASYNC OPEN IGNORES nag_Node AND nag_Line. The node is selected
*       afterwards with a LINK command.
*
*       WARNING: THE FIRST LINK MUST WAIT. A freshly spawned client silently
*       DROPS every command until it has signalled that it is up, so a help
*       request that opens the client and links in one breath does nothing
*       at all and only the second press works. The ActiveToolID/ToolStatusID
*       message is that signal: Help_Show remembers the node, and
*       Help_HandleMessages fires the LINK when the message arrives.
*
*       WARNING: struct NewAmigaGuide MUST STAY VALID until the client has
*       acknowledged it, so it lives in the context here and never on a
*       caller's stack.
*
*       CloseAmigaGuide closes the client's own windows, so it has to run
*       before the screen they are on is closed.
*
****************************************************************************/

#ifndef WAVESMITH_HELP_H
#define WAVESMITH_HELP_H

#include <exec/types.h>
#include <libraries/amigaguide.h>   /* struct NewAmigaGuide; pulls in
                                     * intuition/screens.h for struct Screen */

/* A node name is the guide's own @node name, the same in every language.
 * Every one the program names is written HELP_NODE("Name"):
 * tools/check_helpnodes.py finds them by it and fails the build when a
 * guide has no such page -- AmigaGuide would quietly show the first one. */
#define HELP_NODE_MAX 32
#define HELP_NODE(n)  ((CONST_STRPTR)(n))

struct HelpContext
{
    struct NewAmigaGuide hc_Nag;        /* read by the guide process until it
                                         * acknowledges: must outlive the call */
    AMIGAGUIDECONTEXT    hc_Handle;     /* NULL until the client is open      */
    struct Screen       *hc_Screen;     /* SET BY THE CALLER before Help_Show;
                                         * NULL = the default public screen   */
    BOOL                 hc_Ready;      /* it has signalled that it is up     */
    char                 hc_Pending[HELP_NODE_MAX]; /* a LINK waiting for that
                                         * signal; "" when there is none      */
    char                 hc_Path[128];  /* the guide: nag_Name points here    */
};

/* Open the manual at `node` (NULL or "" = Main), opening the client the
 * first time, on hc_Screen. FALSE means there is no help to show, which is
 * not a failure.
 *
 * The SCREEN is a field rather than an argument -- Herald's shape -- so
 * the call stays inside the register budget (2 pointers, 2 integers) with
 * the node in it. Context help is what the node is for: the Help key over a
 * control opens the page that explains that control. */
BOOL  Help_Show(struct HelpContext *help, CONST_STRPTR node);

/* Signal bits for the Wait() mask; 0 while the client is not open. */
ULONG Help_Signal(struct HelpContext *help);

/* Reply everything queued, and fire a deferred LINK once the client is up.
 * A cheap no-op while it is not open. */
void  Help_HandleMessages(struct HelpContext *help);

/* Close the client and its windows, keeping the library for a re-open. */
void  Help_Close(struct HelpContext *help);

/* Close it and drop the library. Once, at the end. */
void  Help_Cleanup(struct HelpContext *help);

#endif /* WAVESMITH_HELP_H */
