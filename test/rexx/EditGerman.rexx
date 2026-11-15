/* EditGerman.rexx -- the autotest EditGerman's driver: WaveSmith in
   German (its catalog put beside the test build for this run), the
   widest words of the bar (Laenge, Kuerzen...) on PAL hires at Topaz 8.
   The boxes from its trace (DEV:Test/german.trace):
     A  the main window: every gadget inside it, the bar's keys left to
        right without one over the next (a row squeezed below its least
        width runs past the window or onto itself)
     B  the Trim window (Right-Amiga T): its gadgets inside it
     C  the Trim window closed, Quit */

options results
signal on syntax
call open(log, 'DEV:Test/auto.log', 'A')
if ~show('L', 'rexxsupport.library') then call addlib('rexxsupport.library', 0, -30, 0)

box. = ''
trim. = ''
last. = ''
n. = 0

do i = 1 to 120 while trim.PLAN = ''
	call delay(50)
	call readtrace
end
if box.WINDOW = '' | box.DECK = '' then do
	call writeln(log, 'FAILED no layout in DEV:Test/german.trace')
	exit 20
end
if trim.PLAN = '' then call writeln(log, 'FAILED no plan after 60 s (the grab shows no length)')
call writeln(log, 'A: the main window in German')
parse var box.WINDOW . . wwid .
wide = 0
names = 'overview wave scroller deck loopkey flagin flagout len trimbutton lensout lensin lensall cancelbutton'
do gi = 1 to words(names)
	g = upper(word(names, gi))
	parse var box.g gx gy gw gh
	if ~datatype(gx, 'W') then do
		call writeln(log, 'FAILED' g 'not in the trace')
		wide = 1
	end
	else if gx + gw > wwid - 4 then do
		call writeln(log, 'FAILED' g 'runs past the window:' gx '+' gw '>' wwid - 4)
		wide = 1
	end
end
if ~wide then call writeln(log, 'ok   every gadget inside the window (' || wwid 'wide)')
/* the bar, left to right: each key where the one before it ends or
   further right */
bar = 'deck loopkey flagin flagout len trimbutton lensout lensin lensall'
over = 0
parse var box.DECK px . pw .
do gi = 2 to words(bar)
	g = upper(word(bar, gi))
	parse var box.g gx . gw .
	if datatype(gx, 'W') & datatype(px, 'W') then
		if gx < px + pw then do
			call writeln(log, 'FAILED' g 'at' gx 'over the key before it (to' px + pw')')
			over = 1
		end
	px = gx
	pw = gw
end
if ~over then call writeln(log, 'ok   the bar left to right, no key over another (to' px + pw')')
address command 'DEV:Test/ScreenInfo >>DEV:Test/auto.log'
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-german.ppm'

call writeln(log, 'B: the Trim window in German')
w0 = n.TRIMWIN
address command 'DEV:Test/InputPoke WIN=Fit.wav KEY=20 QUAL=128'
do i = 1 to 20 while n.TRIMWIN = w0
	call delay(10)
	call readtrace
end
if n.TRIMWIN = w0 then call writeln(log, 'FAILED Right-Amiga T: no Trim window in the trace')
else do
	parse var trim.TRIMWIN . . tww .
	wide = 0
	names = 'trimin triminback triminon triminlisten triminfade triminshow trimout trimoutback trimouton trimoutlisten trimoutfade trimoutshow'
	do gi = 1 to words(names)
		g = upper(word(names, gi))
		parse var box.g gx gy gw gh
		if ~datatype(gx, 'W') then do
			call writeln(log, 'FAILED' g 'not in the trace')
			wide = 1
		end
		else if gx + gw > tww - 4 then do
			call writeln(log, 'FAILED' g 'runs past the Trim window:' gx '+' gw '>' tww - 4)
			wide = 1
		end
	end
	if ~wide then call writeln(log, 'ok   every gadget inside the Trim window (' || tww 'wide)')
	call delay(25)
	address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-german-trim.ppm'
end

call writeln(log, 'C: Trim window closed, Quit')
/* its close gadget; the title "Kuerzen" found by a part without the
   umlaut */
address command 'DEV:Test/InputPoke WIN=rzen X=8 Y=5 STEPS=4'
call delay(30)
address command 'DEV:Test/InputPoke WIN=Fit.wav KEY=16 QUAL=128'
call delay(250)
address command 'Status >NIL: COMMAND=RAM:German/WaveSmith'
if rc = 0 then call writeln(log, 'FAILED Quit: still running')
else call writeln(log, 'ok   Quit')
exit 0

syntax:
	call writeln(log, 'FAILED ARexx error' rc '(' || errortext(rc) || ') in line' sigl)
	exit 20

/* the trace: four numbers after a name are a box; eight, a handle line
   or a window's box (its last four) */
readtrace: procedure expose box. trim. last. n.
	n. = 0
	if ~open(tr, 'DEV:Test/german.trace', 'R') then return
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
