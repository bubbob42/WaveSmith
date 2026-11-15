/* EditHelp.rexx -- the autotest EditHelp's driver: WaveSmith's manual,
   worked by InputPoke at the places its trace (DEV:Test/help.trace) says.
   The test disk speaks German: the German manual opens, its pages titled
   "WaveSmith: Die Wellenform" and so on.
     A  Help over the waveform: "Waveform"; the manual's window titled
        "Die Wellenform" -- the first LINK, which the new client drops
        unless it is sent once the client has said it is up
     B  the manual behind (its depth gadget), WaveSmith active again (its
        title bar), Help over Trim...: "Trimming"; the same window now
        at "Kuerzen", "Die Wellenform" gone
     C  the same over the deck, the Trim window's In (Right-Amiga T) and
        Cancel: "Listening", "Trimming", "Window"; the window at
        "Anhoeren", "Kuerzen", "Das Fenster"
     D  Project > Help...: "Main"; "Das Fenster" gone
     E  Quit: WaveSmith gone, and its manual's window with it */

options results
signal on syntax
call open(log, 'DEV:Test/auto.log', 'A')
if ~show('L', 'rexxsupport.library') then call addlib('rexxsupport.library', 0, -30, 0)

box. = ''
trim. = ''
last. = ''

do i = 1 to 120 while trim.PLAN = ''
	call delay(50)
	call readtrace
end
if trim.PLAN = '' then do
	call writeln(log, 'FAILED no plan after 60 s')
	exit 20
end

call writeln(log, 'A: Help over the waveform')
call help box.wave, 'Waveform'
if ~waitwin('Wellenform', 30) then call writeln(log, 'FAILED no manual window at "Die Wellenform" after 15 s')
else call writeln(log, 'ok   the manual at "Die Wellenform"')
address command 'DEV:Test/ScreenInfo >>DEV:Test/auto.log'
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-help-wave.ppm'

call writeln(log, 'B: Help over Trim..., the manual behind first')
if ~behind('Wellenform') then call writeln(log, 'FAILED the manual could not be sent behind')
call help box.trimbutton, 'Trimming'
if ~waitwin('rzen', 10) then call writeln(log, 'FAILED the manual not at "Kuerzen"')
else call writeln(log, 'ok   the manual at "Kuerzen"')
if there('Wellenform') then call writeln(log, 'FAILED "Die Wellenform" still there: a second window, not the page changed')
else call writeln(log, 'ok   one manual window, its page changed')
address command 'DEV:Test/ScreenInfo >>DEV:Test/auto.log'
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-help-save.ppm'

call writeln(log, 'C: Help over the deck, In and Cancel')
/* the pages' titles by a part without an umlaut ("Anh" for
   Anhoeren, "rzen" for Kuerzen): the umlaut stays out of this file */
call behind 'rzen'
call help box.deck, 'Listening'
if ~waitwin('Anh', 10) then call writeln(log, 'FAILED the manual not at "Anhoeren"')
else call writeln(log, 'ok   the manual at "Anhoeren"')
call behind 'Anh'
/* In's time is the Trim window's (Right-Amiga T; titled "Trim": the
   program's own words stay English on this disk, its catalog is not
   beside the build): Help over it there; the manual's page waited for by
   "rzen" as before (a German Trim window, "Kuerzen", would hold it too) */
w0 = n.TRIMWIN
address command 'DEV:Test/InputPoke WIN=Hilfe.wav KEY=20 QUAL=128'
do i = 1 to 20 while n.TRIMWIN = w0
	call delay(10)
	call readtrace
end
if box.trimin = '' then call writeln(log, 'FAILED Right-Amiga T: no Trim window in the trace')
call help box.trimin, 'Trimming', 'Trim'
if ~waitwin('rzen', 10) then call writeln(log, 'FAILED the manual not at "Kuerzen"')
else call writeln(log, 'ok   the manual at "Kuerzen"')
call behind 'rzen'
call help box.cancelbutton, 'Window'
if ~waitwin('Fenster', 10) then call writeln(log, 'FAILED the manual not at "Das Fenster"')
else call writeln(log, 'ok   the manual at "Das Fenster"')

call writeln(log, 'D: Project > Help...')
call behind 'Fenster'
parse var last.HELPITEM . . . tx ix iy
n0 = n.HELPNODE
call pick tx ix iy
do i = 1 to 20 while n.HELPNODE = n0
	call delay(10)
	call readtrace
end
parse var last.HELPNODE node shown
if n.HELPNODE ~= n0 + 1 | node ~= 'Main' | shown ~= 1 then call writeln(log, 'FAILED Project > Help...: "'last.HELPNODE'"')
else call writeln(log, 'ok   Project > Help...: Main')
call delay(150)
if there('Fenster') then call writeln(log, 'FAILED "Das Fenster" still there after Project > Help...')
else call writeln(log, 'ok   the manual at its first page')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-help-main.ppm'

call writeln(log, 'E: Quit')
address command 'DEV:Test/InputPoke WIN=Hilfe.wav KEY=16 QUAL=128'
call delay(250)
address command 'Status >NIL: COMMAND=DEV:sasc/WaveSmith'
if rc = 0 then call writeln(log, 'FAILED Quit: still running')
else call writeln(log, 'ok   Quit')
if there('WaveSmith') then call writeln(log, 'FAILED a window titled WaveSmith is still there: the manual not closed')
else call writeln(log, 'ok   the manual closed with it')
exit 0

syntax:
	call writeln(log, 'FAILED ARexx error' rc '(' || errortext(rc) || ') in line' sigl)
	exit 20

/* the pointer over a box of WaveSmith's window, the Help key; the
   trace's helpnode line checked */
help: procedure expose box. trim. last. n. log
	parse arg l t w h, want, win
	if win = '' then win = 'Hilfe.wav'
	n0 = n.HELPNODE
	address command 'DEV:Test/InputPoke WIN='win' X='l + w % 2' Y='t + h % 2' CLICKS=0 STEPS=4'
	call delay(10)
	address command 'DEV:Test/InputPoke WIN='win' KEY=95'
	do i = 1 to 20 while n.HELPNODE = n0
		call delay(10)
		call readtrace
	end
	parse var last.HELPNODE node shown
	if n.HELPNODE ~= n0 + 1 then call writeln(log, 'FAILED Help over' want': no helpnode line')
	else if node ~= want | shown ~= 1 then call writeln(log, 'FAILED Help over' want': "'last.HELPNODE'"')
	else call writeln(log, 'ok   Help over' want)
	return

/* a window whose title holds part, on the front screen */
there: procedure
	parse arg part
	address command 'DEV:Test/InputPoke >NIL: WIN='part
	return rc = 0

/* waited for, half a second a try */
waitwin: procedure
	parse arg part, tries
	do i = 1 to tries
		if there(part) then return 1
		call delay(25)
	end
	return 0

/* the manual's window sent behind by its depth gadget, WaveSmith's
   window active again by its title bar */
behind: procedure
	parse arg part
	address command 'DEV:Test/InputPoke >T:edithelp.box WIN='part
	if rc ~= 0 then return 0
	if ~open(bx, 'T:edithelp.box', 'R') then return 0
	line = readln(bx)
	call close(bx)
	parse var line 'window at ' l ',' t ', ' w 'x' h
	address command 'DEV:Test/InputPoke X='l + w - 8' Y='t + 4
	call delay(25)
	address command 'DEV:Test/InputPoke WIN=Hilfe.wav X=80 Y=4'
	call delay(25)
	return 1

pick: procedure
	parse arg tx ix iy
	address command 'DEV:Test/InputPoke X='tx' Y=4 MENU=1'
	call delay(15)
	address command 'DEV:Test/InputPoke X='ix' Y='iy' MENU=2'
	call delay(15)
	address command 'DEV:Test/InputPoke MENU=0'
	call delay(25)
	return

readtrace: procedure expose box. trim. last. n.
	n. = 0
	if ~open(tr, 'DEV:Test/help.trace', 'R') then return
	do while ~eof(tr)
		line = readln(tr)
		parse var line what rest
		what = upper(what)
		n.what = n.what + 1
		last.what = rest
		if words(rest) = 8 then do
			parse var rest . . . . a b c d
			trim.what = a b c d
		end
		else if words(rest) = 4 & datatype(word(rest, 1), 'W') then box.what = rest
	end
	call close(tr)
	return
