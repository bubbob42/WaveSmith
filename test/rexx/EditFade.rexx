/* EditFade.rexx -- the autotest EditFade's driver: WaveSmith's Effects
   menu (concept 3.2) worked by InputPoke at the places its trace
   (DEV:Test/fade.trace) says:
     A  the suggestion there; a part marked by a drag (columns 100 .. 200)
     B  Right-Amiga F: faded in, Even (the settings' shape at first), the
        part's edges on zero crossings, three pieces, changed
     C  another part (columns 400 .. 480), Effects/Fade out from the
        menu: five pieces
     D  Undo: three pieces again; Redo: five
     D2 nothing marked (a click): Right-Amiga F, a fade at In as long as
        the lead; Right-Amiga Shift F, one at Out as long as the tail;
        Undo: the fade at Out gone; Redo: back
     E  Save (Right-Amiga S): saved with both end fades, nothing unsaved
     F  Quit (Right-Amiga Q): nothing asked
   German keymap: F raw 35, S 33, Z 21, Y 49, Q 16. */

options results
signal on syntax
call open(log, 'DEV:Test/auto.log', 'A')
if ~show('L', 'rexxsupport.library') then call addlib('rexxsupport.library', 0, -30, 0)

box. = ''
trim. = ''
one. = ''
last. = ''
selected = ''

call writeln(log, 'A: the suggestion, a part marked')
do i = 1 to 120 while trim.PLAN = ''
	call delay(50)
	call readtrace
end
if trim.PLAN = '' then do
	call writeln(log, 'FAILED no plan after 60 s')
	exit 20
end
parse var box.wave wl wt ww wh
ky = wt + wh % 2
fc = one.FIRSTCOL
call mark wl + fc + 100, wl + fc + 200, ky
call readtrace
parse var selected sa sb
if sa = '' | sb <= sa then call writeln(log, 'FAILED no part marked ("'last.SELECTED'")')
else call writeln(log, 'ok   marked' sa'..'sb)

call writeln(log, 'B: Right-Amiga F, Fade in')
f0 = n.FADE
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=35 QUAL=128'
call delay(100)
call readtrace
parse var trim.FADE fa fb fo fs
parse var last.TIMELINE . . tp tf
if n.FADE ~= f0 + 1 then call writeln(log, 'FAILED Fade in: no fade line')
else if fo ~= 0 | fs ~= 1 then call writeln(log, 'FAILED Fade in: out' fo', shape' fs', not 0, Even (1)')
else if abs(fa - sa) > 4000 | abs(fb - sb) > 4000 then call writeln(log, 'FAILED Fade in at' fa'..'fb', not the part marked' sa'..'sb)
else if tp ~= 3 then call writeln(log, 'FAILED Fade in:' tp 'pieces, not 3')
else if one.DIRTY ~= 1 then call writeln(log, 'FAILED Fade in: not marked changed')
else call writeln(log, 'ok   faded in' fa'..'fb', Even: 3 pieces, changed')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-fade-in.ppm'

call writeln(log, 'C: Effects/Fade out from the menu')
call mark wl + fc + 400, wl + fc + 480, ky
call readtrace
parse var selected sa sb
f0 = n.FADE
parse var last.FADEOUTITEM . . . tx ix iy
call pick tx ix iy
call delay(50)
call readtrace
parse var trim.FADE fa fb fo fs
parse var last.TIMELINE . . tp tf
if n.FADE ~= f0 + 1 then call writeln(log, 'FAILED Fade out: no fade line (menu at' last.FADEOUTITEM')')
else if fo ~= 1 then call writeln(log, 'FAILED Fade out: out' fo', not 1')
else if abs(fa - sa) > 4000 | abs(fb - sb) > 4000 then call writeln(log, 'FAILED Fade out at' fa'..'fb', not the part marked' sa'..'sb)
else if tp ~= 5 then call writeln(log, 'FAILED Fade out:' tp 'pieces, not 5')
else call writeln(log, 'ok   faded out' fa'..'fb': 5 pieces')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-fade-out.ppm'

call writeln(log, 'D: Undo, Redo')
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=21 QUAL=128'
call delay(80)
call readtrace
parse var last.TIMELINE . . up .
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=49 QUAL=128'
call delay(80)
call readtrace
parse var last.TIMELINE . . rp .
if up ~= 3 then call writeln(log, 'FAILED Undo of the fade out:' up 'pieces, not 3')
else if rp ~= 5 then call writeln(log, 'FAILED Redo of the fade out:' rp 'pieces, not 5')
else call writeln(log, 'ok   Undo: 3 pieces, Redo: 5')

call writeln(log, 'D2: nothing marked, the fades at In and Out')
call mark wl + fc + 300, wl + fc + 300, ky
call readtrace
parse var selected sa sb
e0 = n.ENDFADE
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=35 QUAL=128'
call delay(100)
call readtrace
parse var last.ENDFADE . . eo ei
if n.ENDFADE ~= e0 + 1 then call writeln(log, 'FAILED Right-Amiga F, nothing marked: no end fade (selected' sa sb')')
else if eo ~= 0 | ei < 15000 | ei > 30000 then call writeln(log, 'FAILED the fade at In: "'last.ENDFADE'", not out 0 and the lead (about 22932)')
else call writeln(log, 'ok   the fade at In:' ei 'frames, the lead')
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=35 QUAL=129'
call delay(100)
call readtrace
parse var last.ENDFADE . . eo ef
parse var last.ENDFADES . . fi fo
if n.ENDFADE ~= e0 + 2 then call writeln(log, 'FAILED Right-Amiga Shift F, nothing marked: no end fade')
else if eo ~= 1 | ef < 90000 | ef > 110000 then call writeln(log, 'FAILED the fade at Out: "'last.ENDFADE'", not out 1 and the tail (about 98784)')
else if fi ~= ei | fo ~= ef then call writeln(log, 'FAILED the picture drawn with "'last.ENDFADES'", not' ei ef)
else call writeln(log, 'ok   the fade at Out:' ef 'frames, the tail; the picture drawn with both')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-fade-ends.ppm'
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=21 QUAL=128'
call delay(80)
call readtrace
parse var last.ENDFADES . . ui uo
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=49 QUAL=128'
call delay(80)
call readtrace
parse var last.ENDFADES . . ri ro
if ui ~= ei | uo ~= 0 then call writeln(log, 'FAILED Undo of the fade at Out: "'ui uo'", not' ei '0')
else if ri ~= ei | ro ~= ef then call writeln(log, 'FAILED Redo of the fade at Out: "'ri ro'", not' ei ef)
else call writeln(log, 'ok   Undo: the fade at Out gone; Redo: back')

call writeln(log, 'E: Save')
s0 = n.SAVED
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=33 QUAL=128'
do i = 1 to 60 while n.SAVED = s0
	call delay(25)
	call readtrace
end
/* "dirty 0" follows "saved" a few lines on: waited for */
do i = 1 to 20 while n.SAVED > s0 & one.DIRTY ~= 0
	call delay(10)
	call readtrace
end
parse var trim.SAVED si so se
if n.SAVED ~= s0 + 1 then call writeln(log, 'FAILED Save: no saved line')
else if se ~= 0 then call writeln(log, 'FAILED Save: error' se)
else if one.DIRTY ~= 0 then call writeln(log, 'FAILED Save: still marked changed')
else call writeln(log, 'ok   saved' si'..'so', nothing unsaved')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-fade-saved.ppm'

call writeln(log, 'F: Quit')
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=16 QUAL=128'
call delay(150)
address command 'Status >NIL: COMMAND=DEV:sasc/WaveSmith'
if rc = 0 then call writeln(log, 'FAILED Quit: still running')
else call writeln(log, 'ok   Quit: nothing asked')
exit 0

syntax:
	call writeln(log, 'FAILED ARexx error' rc '(' || errortext(rc) || ') in line' sigl)
	exit 20

mark: procedure
	parse arg x1, x2, y
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='x1' Y='y' CLICKS=0 STEPS=4'
	call delay(25)
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='x1' Y='y' SELECT=1'
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='x2' Y='y' SELECT=2 STEPS=8'
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='x2' Y='y' SELECT=0 STEPS=1'
	call delay(50)
	return

pick: procedure
	parse arg tx ix iy
	address command 'DEV:Test/InputPoke X='tx' Y=4 MENU=1'
	call delay(15)
	address command 'DEV:Test/InputPoke X='ix' Y='iy' MENU=2'
	call delay(15)
	address command 'DEV:Test/InputPoke MENU=0'
	call delay(25)
	return

readtrace: procedure expose box. trim. one. last. n. selected
	n. = 0
	if ~open(tr, 'DEV:Test/fade.trace', 'R') then return
	do while ~eof(tr)
		line = readln(tr)
		parse var line what rest
		what = upper(what)
		n.what = n.what + 1
		last.what = rest
		if what = 'SELECTED' then selected = subword(rest, 3, 2)
		if words(rest) = 1 then one.what = strip(rest)
		else if words(rest) = 6 then do
			parse var rest . . . a b c
			trim.what = a b c
		end
		else if words(rest) = 8 then do
			parse var rest . . . . a b c d
			trim.what = a b c d
		end
		else if words(rest) = 4 & datatype(word(rest, 1), 'W') then box.what = rest
	end
	call close(tr)
	return
