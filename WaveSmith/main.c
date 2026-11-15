/*
 * WaveSmith -- a recording's ends trimmed by eye and ear
 * (Documentation/Concept-WaveSmith-2026-10.md).
 *
 *   WaveSmith [FILE] <file> [PUBSCREEN <name>]
 *
 * From Workbench: a project icon's default tool, or WaveSmith's own icon
 * with a file icon shift-clicked. The window reads the file and suggests
 * In and Out (the lead-in and the run-out found); the handles are dragged,
 * Save writes the file anew without the parts outside them.
 *
 * Return codes: 0 the window was closed, 20 no window (a library or a
 * class missing: said in the Shell).
 *
 * WARNING: main takes NO parameters. libnix's startup calls it with the
 * stack convention while this build is -msasregparm; argc and argv would
 * come from the wrong registers (the command line was ignored). A
 * Workbench start is told by pr_CLI, its message is _WBenchMsg
 * (PrismaControl's prefsmain.c).
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <workbench/startup.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "gui.h"
#include "wavesmith_locale.h"
#include "doc.h"
#include "textutil.h"
#include "WaveSmith_rev.h"

const char version[] = VERSTAG;

extern struct WBStartup *_WBenchMsg;

#define TEMPLATE	"FILE,PUBSCREEN/K"
#define GUI_STACK	32768L		/* ReAction's layout is generous with stack */

enum { A_FILE, A_PUBSCREEN, A_COUNT };

/* Everything the program does, on a stack of its own: only statics cross
   StackSwap() (PrismaModes' main.c) */
static struct {
	CONST_STRPTR file, pubscreen;
	LONG rc;
	char wbpath[DOC_PATHLEN];
} pe;

static APTR pe_stack;
static struct StackSwapStruct pe_sss;

/* WARNING: NOINLINE, and not decoration. With -Os GCC inlines program()
 * into the wrapper, whose frame is then built on the OLD stack before
 * StackSwap moves the stack pointer -- every local of it on a stack that
 * is no longer the current one. The family has shipped that crash before. */
#ifdef __GNUC__
# define NOINLINE __attribute__((noinline))
#else
# define NOINLINE
#endif

static void program(void)
{
	WS_OpenCatalog();		/* none: English (the built-in strings) */
	if (!Gui_OpenLibs()) {
		if (Output()) {
			PutStr((STRPTR) "WaveSmith: needs ");
			PutStr((STRPTR) (Gui_Missing() ? Gui_Missing() : (CONST_STRPTR) "window.class"));
			PutStr((STRPTR) " (V44, ReAction)\n");
		}
		pe.rc = RETURN_FAIL;
	} else {
		pe.rc = Gui_Run(pe.file, pe.pubscreen);
	}
	Gui_CloseLibs();
	WS_CloseCatalog();
}

static NOINLINE void run_swapped(void)
{
	StackSwap(&pe_sss);
	program();
	StackSwap(&pe_sss);
}

/* the file a Workbench start brings (a project icon, or one shift-clicked) */
static void wb_file(struct WBStartup *wbs)
{
	struct WBArg *wa;

	if (!wbs || wbs->sm_NumArgs < 2) return;
	wa = &wbs->sm_ArgList[1];
	if (!wa->wa_Lock || !NameFromLock(wa->wa_Lock, (STRPTR) pe.wbpath, DOC_PATHLEN)) return;
	if (AddPart((STRPTR) pe.wbpath, wa->wa_Name, DOC_PATHLEN)) pe.file = (CONST_STRPTR) pe.wbpath;
}

int main(void)
{
	struct RDArgs *rda = NULL;
	LONG args[A_COUNT];
	struct Task *me;
	int i;

	for (i = 0; i < A_COUNT; i++) args[i] = 0;
	me = FindTask(NULL);
	if (((struct Process *) me)->pr_CLI) {
		if (!(rda = ReadArgs((STRPTR) TEMPLATE, args, NULL))) {
			PrintFault(IoErr(), (STRPTR) "WaveSmith");
			return RETURN_ERROR;
		}
		pe.file = (CONST_STRPTR) args[A_FILE];
		pe.pubscreen = (CONST_STRPTR) args[A_PUBSCREEN];
	} else {
		wb_file(_WBenchMsg);
	}

	if ((pe_stack = AllocVec(GUI_STACK, MEMF_ANY))) {
		pe_sss.stk_Lower = pe_stack;
		pe_sss.stk_Upper = (ULONG) ((UBYTE *) pe_stack + GUI_STACK);
		pe_sss.stk_Pointer = (APTR) pe_sss.stk_Upper;
		run_swapped();
		FreeVec(pe_stack);
	} else {
		/* a caller with as much is fine; else no window on 4 KB */
		if ((ULONG) me->tc_SPUpper - (ULONG) me->tc_SPLower >= (ULONG) GUI_STACK)
			program();
		else
			pe.rc = RETURN_FAIL;
	}
	if (rda) FreeArgs(rda);
	return (int) pe.rc;
}
