/* EditTrim.rexx -- the autotest EditTrim's driver: WaveSmith's window
   worked by InputPoke at the places its trace (ENV:WaveSmithTrace,
   DEV:Test/edit.trace) says -- the gadgets' boxes in the window, the
   view after At In -- each step grabbed:
     A  the suggestion          (edit-a)
     H  the bubbles: over In "In 0:03.600", over the sound its time,
        over the ruler none (the trace's help lines); Save's (edit-h,
        edit-h2)
     W  the wheel: a notch zooms in where the pointer is, one zooms back
        out, Shift scrolls, a notch over the overview goes there (edit-w)
     I  Settings, Lead and tail...: the lead's field emptied, 1500 typed,
        Use: In a second earlier, Out where it was (the handles untouched:
        the suggestion made again; edit-i the window); again, 900 and Esc:
        nothing changes; again, 500, Return twice: In back
     B  At In: 3 s around In    (edit-b)
     C  In dragged 40 pixels to the right, let go: snapped (edit-c)
     D  All, then a Save (Right-Amiga S) that cannot write -- a drawer
        has the temporary name: the handles must stay where C left them
        (edit-d)
     X  Save, Cancel at once: the save called off, the file as it was
     E  the drawer gone, Save: the file anew, read again (edit-e)
     P  At In, Play on the simulated player (ENV:WaveSmithSimPlay): the
        view follows the play head a page on (edit-p2), the pages read
        ahead (no fetch at a turn)
     S  Space pauses, Space goes on from the very frame, the Play button
        pauses (edit-s), Stop: the marker back at the start
     K  a click on the sound while playing: the card goes on from there;
        stopped: the play head there, Space plays from it
     Z  a part marked by a drag, View / Zoom to selection: the view holds
        it, zoomed in (edit-z)
     J  >| puts the play marker at the end, in view (edit-j); |< at the
        start
     Q  the arrows and the Play menu: a click on the sound, Right three
        times: the play marker three columns on; In dragged (it is the
        arrows' then), Right three times, Shift Right, Ctrl Left: In a
        column each, a page, the start -- one Undo takes the whole run
        back to the drag, another to the suggestion; Up zooms in with In
        in view, Down out; a short part marked, Shift Space plays just it;
        Play / Loop? on: played out, it plays again (looped), Stop; off
     O  Out's line in the overview strip (lines there, no flags)
        dragged 20 points left: Out snapped some 20 overview columns
        earlier (edit-o)
     U  Undo (Right-Amiga Z): In and Out the suggestion's again, the *
        gone; Redo (Right-Amiga Y): O's Out back, the * too; Undo again
        (edit-u)
     L  All, a part marked, Cut out (Right-Amiga X), Join: 2 pieces, the
        timeline the cut shorter (edit-l); Undo: the whole file; Redo
     M  another part, Cut out, Silence (S): 4 pieces, the length kept
        (edit-m); G's save writes it -- the Mac's check replays the
        trace (make_edit_single.py --trace)
     V  a click puts the play marker, Project / Split at the cursor:
        Trim-1.wav and Trim-2.wav beside it, the doc still changed
        (edit-v; the check replays the parts too)
     R  the weight bar between the overview and the waveform dragged 30
        pixels down: the overview that much taller, the waveform that
        much shorter (edit-r); dragged back up: the two as they were
     Y  Save as (Right-Amiga A): the requester's name emptied, TrimAs.wav
        typed, Return: written (its icon and map too), the window reads it
        -- the doc TrimAs.wav from here (edit-y the requester); Trim.wav
        stays E's (the Mac's check replays both)
     T  Settings: Snap to zero crossings? off -- In's line dragged, not
        snapped (the same drag snapped first: the control); on again; Keep
        a backup? on, Save settings: ENVARC:WaveSmith.prefs says so (G's
        save then keeps TrimAs.wav.bak, the file Y made, and TrimAs.prl.bak)
     N  the Trim window, Edit / Trim... (Right-Amiga T): open, its twelve
        gadgets traced; Show In puts the view at In; In's arrow back
        moves In one step of the zoom; Undo puts it back; its close
        gadget closes it
     F  the handle flags: In picked at first; Out's flag picks Out and
        shows it, In's flag In again; a double-click on In's flag on the
        sound opens the Trim window (closed again)
     G  In dragged by its flag (the title's *), Right-Amiga Q asks in a window of
        WaveSmith's own, centred on its window (edit-g): Esc (Cancel,
        the rightmost) keeps it running, changed; asked again, Return
        (Save, the leftmost) saves and quits */

options results
signal on syntax
call open(log, 'DEV:Test/auto.log', 'A')
if ~show('L', 'rexxsupport.library') then call addlib('rexxsupport.library', 0, -30, 0)

box. = ''
trim. = ''
one. = ''
last. = ''
helptext = ''
pausedat = ''
playat = ''
seekat = ''
beats = ''
savederr = ''
answer = ''
selected = ''
menuat = ''
view = ''
read = ''
call readtrace
if box.wave = '' | box.deck = '' then do
	call writeln(log, 'FAILED no layout in DEV:Test/edit.trace')
	exit 20
end
call writeln(log, 'wave' box.wave 'deck' box.deck 'lensall' box.lensall)
/* every gadget inside the window: a row too wide runs past its edge */
parse var box.window wx0 wy0 wwid whei
wide = 0
do gi = 1 to 13
	/* upper-cased: a tail from a variable is taken as it is, and the
	   trace's names were stored upper-cased (readtrace) */
	g = upper(word('overview wave scroller deck loopkey flagin flagout len trimbutton lensout lensin lensall cancelbutton', gi))
	parse var box.g gx gy gw gh
	/* REXX does not stop at a false '&': the sum only for a box there is */
	if datatype(gx, 'W') then
		if gx + gw > wwid - 4 then do
			call writeln(log, 'FAILED' g 'runs past the window:' gx '+' gw '>' wwid - 4)
			wide = 1
		end
end
if ~wide then call writeln(log, 'ok   every gadget inside the window (' || wwid 'wide)')
/* no settings file (the test's prologue deleted them): the defaults */
/* (ARexx's SPACE() without its count takes every blank out) */
if space(trim.PREFS, 1) ~= '1 500 2000 0' | space(trim.PREFS2, 1) ~= '1 10 0' then
	call writeln(log, 'FAILED the settings at the start:' space(trim.PREFS, 1) '/' space(trim.PREFS2, 1)', not the defaults')
else call writeln(log, 'ok   the settings at the start: the defaults, from no file')

/* the reading done: the plan and the reading's line in the trace (up
   to 150 s; the plan comes first) */
do i = 1 to 150 while trim.PLAN = '' | read = ''
	call delay(50)
	call readtrace
end
if trim.PLAN = '' then do
	call writeln(log, 'FAILED no plan in the trace after 150 s: the reading never ended')
	address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-a.ppm'
	exit 20
end
call writeln(log, 'plan (in out leadclicks tailclicks):' trim.PLAN 'after' i 's; read (frames ticks):' read)

call writeln(log, 'A: the suggestion')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-a.ppm'

call writeln(log, 'H: the bubbles')
call bubbles

call writeln(log, 'W: the wheel')
call wheel
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-w.ppm'

call writeln(log, 'I: Settings, Lead and tail...')
parse var trim.PLAN pin pout .
k = 'MARGINS'
m0 = n.k
call lead_typed '1 5 10 10', 'RETURN'
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-i.ppm'
parse var box.MARGINSUSE ul ut uw uh
address command 'DEV:Test/InputPoke WIN=Lead X='ul + uw % 2' Y='ut + uh % 2' STEPS=4'
call delay(100)
call readtrace
parse var trim.PLAN nin nout .
if n.k ~= m0 + 1 then call writeln(log, 'FAILED Lead and tail, Use: no margins line (' || n.k - m0')')
else if space(trim.MARGINS, 1) ~= '1500 2000 1' then call writeln(log, 'FAILED Lead and tail, Use: "'space(trim.MARGINS, 1)'", not 1500 2000 moved')
else if nin + 44100 ~= pin | nout ~= pout then call writeln(log, 'FAILED Lead and tail 1500: In' pin '->' nin', Out' pout '->' nout)
else call writeln(log, 'ok   Lead and tail 1500, Use: In' pin '->' nin', a second earlier, Out' nout 'kept')
/* 900, then Esc: Cancel -- nothing changes */
call lead_typed '9 10 10', 'RETURN'
address command 'DEV:Test/InputPoke KEY=69'
call delay(100)
call readtrace
parse var trim.PLAN cin .
if n.k ~= m0 + 2 | last.MARGINS ~= 'kept' then call writeln(log, 'FAILED Lead and tail, Esc: "'last.MARGINS'"')
else if word(space(trim.PREFS, 1), 2) ~= 1500 | cin ~= nin then call writeln(log, 'FAILED Lead and tail, Esc: the lead' word(space(trim.PREFS, 1), 2)', In' cin)
else call writeln(log, 'ok   Lead and tail, 900 and Esc: kept, 1500 still, In' cin)
/* 500, Return in the field, Return again: Use -- In back */
call lead_typed '5 10 10', 'RETURN'
address command 'DEV:Test/InputPoke KEY=68'
call delay(100)
call readtrace
parse var trim.PLAN bin bout .
if n.k ~= m0 + 3 | space(trim.MARGINS, 1) ~= '500 2000 1' then call writeln(log, 'FAILED Lead and tail 500, Return: "'space(trim.MARGINS, 1)'"')
else if bin ~= pin | bout ~= pout then call writeln(log, 'FAILED Lead and tail 500: In' bin', Out' bout', not' pin pout)
else call writeln(log, 'ok   Lead and tail 500, Return: In' bin 'again')

call writeln(log, 'B: At In')
call atin
call delay(100)
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-b.ppm'

call readtrace
if view = '' then do
	call writeln(log, 'FAILED no view after At In in the trace')
	exit 20
end
parse var view vin vout vtop vzoom
if vin = 4294967295 then do
	call writeln(log, 'FAILED no In after At In:' view)
	exit 20
end
parse var box.wave wl wt ww wh
/* px, py: a quote followed by x or b is a hex or binary string ('41'x) */
px = wl + one.FIRSTCOL + (vin - vtop) % vzoom
py = wt + (wh * 2) % 3
call writeln(log, 'C: In at column' px '(in' vin 'top' vtop 'zoom' vzoom') dragged 40 pixels')
address command 'DEV:Test/InputPoke WIN=WaveSmith X='px' Y='py' CLICKS=0 STEPS=4'
call delay(25)
address command 'DEV:Test/InputPoke WIN=WaveSmith X='px' Y='py' SELECT=1'
address command 'DEV:Test/InputPoke WIN=WaveSmith X='px + 40' Y='py' SELECT=2 STEPS=8'
address command 'DEV:Test/InputPoke WIN=WaveSmith X='px + 40' Y='py' SELECT=0 STEPS=1'
call delay(100)
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-c.ppm'
call readtrace
k = 'SNAPPED-IN'
snapped = trim.k
call writeln(log, 'snapped (in out top zoom):' snapped)

call writeln(log, 'D: All, a Save that cannot write')
call click box.lensall
call delay(50)
/* the original delete-protected: the save writes the new file, then
   cannot take the original's place -- it fails, the original as it was */
address command 'Protect DEV:Test/Edit/Trim.wav -d'
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=33 QUAL=128'
call delay(150)
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-d.ppm'
call readtrace
k = 'KEPT'
kept = trim.k
call writeln(log, 'kept (in out top zoom):' kept)
if kept = '' then call writeln(log, 'FAILED no kept line: the save did not fail, or kept nothing')
else if snapped = '' | word(kept, 1) ~= word(snapped, 1) | word(kept, 2) ~= word(snapped, 2) then
	call writeln(log, 'FAILED the handles moved over a failed save')
else call writeln(log, 'ok   the handles kept over a failed save')
address command 'Protect DEV:Test/Edit/Trim.wav +d'
if exists('DEV:Test/Edit/WaveSmith-0.tmp') then call writeln(log, 'FAILED the failed save left its temporary file')

call writeln(log, 'X: Save, Cancel at once: the save called off')
k = 'CANCELLED'
c0 = n.k
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=33 QUAL=128'
call click box.cancelbutton
call delay(150)
call readtrace
if n.k = c0 then call writeln(log, 'FAILED (timing?) the save ended before Cancel was heard')
else if savederr ~= 4294967290 then call writeln(log, 'FAILED Cancel heard, but the save ended' savederr', not called off (-6)')
else call writeln(log, 'ok   Save cancelled: the file as it was')

call writeln(log, 'E: Save')
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=33 QUAL=128'
call delay(500)
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-e.ppm'
call readtrace
if n.SAVING < 3 then call writeln(log, 'FAILED the save told' n.SAVING 'quarters of its progress, not 4')
else call writeln(log, 'ok   the save told its progress:' n.SAVING 'quarters')
/* Create icons? (on by default): the file had none, it has one now */
if n.ICON ~= 1 | word(last.ICON, 2) ~= 1 | ~exists('DEV:Test/Edit/Trim.wav.info') then
	call writeln(log, 'FAILED Save: no icon made for Trim.wav (' || n.ICON 'icon lines, last "'last.ICON'")')
else call writeln(log, 'ok   Save: an icon made for Trim.wav')

call writeln(log, 'P: At In, Play: the view follows the play head')
call atin
call delay(50)
call readtrace
/* v0, p0: a variable named like a tail (VIEWED) would replace it */
v0 = n.VIEWED
p0 = n.PLAY
k = 'FETCHED'
f0 = n.k
call deckkey 1
call delay(250)
call readtrace
/* the deck's second fifth is its Play key (tapedeck.gadget's keys side
   by side): the deck said Play */
parse var last.DECKKEY . . dmode dpause
if dmode ~= 1 | dpause ~= 0 then call writeln(log, 'FAILED the deck''s Play key: "'last.DECKKEY'"')
else call writeln(log, 'ok   the deck''s Play key')
if n.PLAY ~= p0 + 1 then call writeln(log, 'FAILED Play started' n.PLAY - p0 'times, not once')
else if n.VIEWED <= v0 then call writeln(log, 'FAILED the view stayed while the play head ran on (' || n.VIEWED - v0 'view changes)')
else call writeln(log, 'ok   the view followed the play head:' n.VIEWED - v0 'page(s), now' trim.VIEWED)
/* the pages turned were read ahead: no fetch at a turn */
if n.PREFETCHED < 1 then call writeln(log, 'FAILED no page read ahead while playing')
else if n.k ~= f0 then call writeln(log, 'FAILED' n.k - f0 'page(s) fetched at a turn: not read ahead in time')
else call writeln(log, 'ok   the pages read ahead:' n.PREFETCHED 'windows handed, no fetch at a turn')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-p2.ppm'

call writeln(log, 'S: Space pauses, Space goes on, the Play button pauses')
a0 = n.PAUSED
p0 = n.PLAY
k = 'HEAD'
h0 = n.k
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=64'
call delay(50)
call readtrace
if n.PAUSED ~= a0 + 1 then call writeln(log, 'FAILED Space while playing paused' n.PAUSED - a0 'times, not once')
else if n.k ~= h0 then call writeln(log, 'FAILED the play head went off at the pause')
else call writeln(log, 'ok   Space paused at frame' pausedat', the head stays')
/* the head's beat while it played (the trace's last beats line) */
parse var beats bn bms
if ~datatype(bn, 'W') | ~datatype(bms, 'W') | bms < 1000 then call writeln(log, 'FAILED no beats line for the play: "'beats'"')
else if bn * 1000 < 25 * bms then call writeln(log, 'FAILED the head moved' bn 'times in' bms 'ms: under 25 a second (the old 10?)')
else call writeln(log, 'ok   the head moved' bn 'times in' bms 'ms (' || bn * 1000 % bms 'a second)')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-s.ppm'
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=64'
call delay(50)
call readtrace
if n.PLAY ~= p0 + 1 then call writeln(log, 'FAILED Space after the pause started' n.PLAY - p0 'plays, not one')
else if playat ~= pausedat then call writeln(log, 'FAILED Space went on from' playat', not from the pause at' pausedat)
else call writeln(log, 'ok   Space went on from the pause, frame' playat)
call delay(50)
call deckkey 4
call delay(50)
call readtrace
if n.PAUSED ~= a0 + 2 then call writeln(log, 'FAILED the deck''s Pause key while playing did not pause')
else call writeln(log, 'ok   the deck''s Pause key paused at frame' pausedat)
call deckkey 3
call delay(50)
call readtrace
if seekat ~= 0 then call writeln(log, 'FAILED Stop left the play marker at' seekat', not at the start')
else call writeln(log, 'ok   Stop: the play marker back at the start')

call writeln(log, 'K: a click on the sound: the card or the play head goes there')
call atin
call delay(50)
call readtrace
parse var view vin vout vtop vzoom
parse var box.wave wl wt ww wh
ky = wt + (wh * 2) % 3
p0 = n.PLAY
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=64'
call delay(20)
call click wl + one.FIRSTCOL + 300 ky 0 0
call delay(30)
call readtrace
want = vtop + 300 * vzoom
if n.PLAY ~= p0 + 2 then call writeln(log, 'FAILED a click while playing started' n.PLAY - p0 - 1 'plays, not one')
else if abs(playat - want) > vzoom then call writeln(log, 'FAILED the card went on from' playat', not from the click at about' want)
else call writeln(log, 'ok   a click while playing: the card went on from' playat '(clicked about' want')')
call deckkey 3
call delay(30)
/* the view anew: playing turned its pages */
call atin
call delay(50)
call readtrace
parse var view vin vout vtop vzoom
s0 = n.SEEK
call click wl + one.FIRSTCOL + 100 ky 0 0
call delay(30)
call readtrace
want = vtop + 100 * vzoom
if n.SEEK ~= s0 + 1 then call writeln(log, 'FAILED a click while stopped put the play head nowhere')
else if abs(seekat - want) > vzoom then call writeln(log, 'FAILED the play head at' seekat', not at the click about' want)
else call writeln(log, 'ok   a click while stopped: the play head at' seekat)
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-k.ppm'
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=64'
call delay(30)
call readtrace
if playat ~= seekat then call writeln(log, 'FAILED Space after the click played from' playat', not from' seekat)
else call writeln(log, 'ok   Space played from the click')
call deckkey 3
call delay(30)

call writeln(log, 'Z: a part marked, View / Zoom to selection')
call atin
call delay(50)
call readtrace
parse var view vin vout vtop vzoom
sx1 = wl + one.FIRSTCOL + 100
sx2 = wl + one.FIRSTCOL + 400
address command 'DEV:Test/InputPoke WIN=WaveSmith X='sx1' Y='ky' CLICKS=0 STEPS=4'
call delay(25)
address command 'DEV:Test/InputPoke WIN=WaveSmith X='sx1' Y='ky' SELECT=1'
address command 'DEV:Test/InputPoke WIN=WaveSmith X='sx2' Y='ky' SELECT=2 STEPS=8'
address command 'DEV:Test/InputPoke WIN=WaveSmith X='sx2' Y='ky' SELECT=0 STEPS=1'
call delay(50)
call readtrace
parse var selected sa sb
/* the views counted through k: n.VIEW would read the variable view */
k = 'VIEW'
v0 = n.k
parse var menuat mtx mix miy
address command 'DEV:Test/InputPoke X='mtx' Y=4 MENU=1'
call delay(15)
address command 'DEV:Test/InputPoke X='mix' Y='miy' MENU=2'
call delay(15)
address command 'DEV:Test/InputPoke MENU=0'
call delay(50)
call readtrace
parse var view zin zout ztop zzoom
cols = ww - 2 * one.FIRSTCOL
if ~datatype(sa, 'W') | ~datatype(sb, 'W') | sb <= sa then call writeln(log, 'FAILED a drag over the sound marked nothing: "'selected'"')
else if n.k ~= v0 + 1 then call writeln(log, 'FAILED Zoom to selection set' n.k - v0 'views, not one (menu at' menuat')')
else if ztop > sa | ztop + cols * zzoom < sb then call writeln(log, 'FAILED the view' ztop '+' cols 'x' zzoom 'does not hold' sa'..'sb)
else if zzoom >= vzoom then call writeln(log, 'FAILED Zoom to selection did not zoom in:' vzoom '->' zzoom)
else call writeln(log, 'ok   Zoom to selection:' sa'..'sb 'in view from' ztop', zoom' vzoom '->' zzoom)
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-z.ppm'

/* Right-Amiga Shift Z: the same by its key (E5) -- the whole file
   first; on the emulator's German keymap Z is the raw key 21; Undo (the
   plain Z) must not answer it */
call click box.lensall
call delay(50)
call readtrace
v1 = n.k
ku = 'UNDONE'
u1 = n.ku
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=21 QUAL=129'
call delay(50)
call readtrace
parse var view z2in z2out z2top z2zoom
if n.ku ~= u1 then call writeln(log, 'FAILED Right-Amiga Shift Z undid instead of zooming')
else if n.k ~= v1 + 1 then call writeln(log, 'FAILED Right-Amiga Shift Z set' n.k - v1 'views, not one')
else if z2zoom ~= zzoom | z2top ~= ztop then call writeln(log, 'FAILED Right-Amiga Shift Z: view from' z2top 'zoom' z2zoom', not the menu''s' ztop zzoom)
else call writeln(log, 'ok   Right-Amiga Shift Z: the same view as the menu''s')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-z2.ppm'

call writeln(log, 'J: the deck''s Forward to the end, Rewind to the start')
/* along the stops (In, the markers, Out, then the file's ends): eight
   presses get past them all; one read of the trace after them. This doc
   is the saved part: In at its start, Out at its end -- one step each
   way (EditMarks steps past markers, In and Out to the ends) */
parse var read frames .
k = 'JUMP'
j0 = n.k
do i = 1 to 8
	call deckkey 2
	call delay(15)
end
call delay(25)
call readtrace
parse var trim.VIEWED jin jout jtop jzoom
if seekat ~= frames then call writeln(log, 'FAILED Forward x 8: the marker at' seekat', not at the end' frames)
else if jtop + cols * jzoom < frames then call writeln(log, 'FAILED Forward: the view' jtop '+' cols 'x' jzoom 'does not show the end')
else call writeln(log, 'ok   Forward: the end in' n.k - j0 'steps, in view')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-j.ppm'
j0 = n.k
do i = 1 to 8
	call deckkey 0
	call delay(15)
end
call delay(25)
call readtrace
if seekat ~= 0 then call writeln(log, 'FAILED Rewind x 8: the marker at' seekat', not at the start')
else call writeln(log, 'ok   Rewind: the start in' n.k - j0 'steps')

call writeln(log, 'Q: the arrows and the Play menu')
call atin
call delay(50)
call readtrace
parse var view vin vout vtop vzoom
parse var box.wave wl wt ww wh
qy = wt + (wh * 2) % 3
call click wl + one.FIRSTCOL + 200 qy 0 0
call delay(50)
call readtrace
q0 = seekat
do i = 1 to 3
	address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=78'
end
call delay(50)
call readtrace
parse var last.ARROW . . qt qf
if qt ~= 0 | qf ~= q0 + 3 * vzoom | seekat ~= qf then
	call writeln(log, 'FAILED Right x3 on the marker: target' qt 'at' qf', the marker' seekat', not' q0 '+ 3 x' vzoom)
else call writeln(log, 'ok   Right x3: the play marker' q0 '->' qf', three columns')
/* In dragged: the arrows' target is In from now */
call drag_in 20
k = 'SNAPPED-IN'
parse var trim.k din .
do i = 1 to 3
	address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=78'
end
call delay(50)
call readtrace
/* (the view as drag_in's At In left it: its zoom the arrows' column) */
parse var view vin vout vtop vzoom
parse var last.ARROW . . qt qf
if qt ~= 1 | qf ~= din + 3 * vzoom then
	call writeln(log, 'FAILED Right x3 on In: target' qt 'at' qf', not' din '+ 3 x' vzoom)
else call writeln(log, 'ok   Right x3 after the drag: In' din '->' qf)
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=78 QUAL=1'
call delay(50)
call readtrace
parse var last.ARROW . . qt qf2
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=79 QUAL=8'
call delay(50)
call readtrace
parse var last.ARROW . . qt qf3
if qf2 <= qf + vzoom | qf3 ~= 0 then call writeln(log, 'FAILED Shift Right, Ctrl Left on In:' qf '->' qf2 '->' qf3)
else call writeln(log, 'ok   Shift Right: In' qf '->' qf2' (a page); Ctrl Left: at 0')
u0 = n.UNDONE
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=21 QUAL=128'
call delay(50)
call readtrace
parse var trim.UNDONE uin .
if n.UNDONE ~= u0 + 1 | uin ~= din then call writeln(log, 'FAILED one Undo after the arrows: In' uin', not the drag''s' din)
else call writeln(log, 'ok   one Undo takes the arrows'' run back: In' uin', where the drag left it')
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=21 QUAL=128'
call delay(50)
call readtrace
parse var trim.UNDONE uin .
if uin ~= word(trim.PLAN, 1) | one.DIRTY ~= 0 then call writeln(log, 'FAILED the second Undo: In' uin', dirty' one.DIRTY)
else call writeln(log, 'ok   another Undo: the suggestion''s In, nothing changed')
/* Up: zoomed in, In (the target) in view; Down: back out */
parse var view vin vout vtop vzoom
z0 = vzoom
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=76'
call delay(50)
call readtrace
parse var trim.KEYZOOM kin kout ktop kzoom
if kzoom >= z0 | kin < ktop | kin >= ktop + cols * kzoom then call writeln(log, 'FAILED Up: zoom' z0 '->' kzoom', In' kin 'top' ktop)
else call writeln(log, 'ok   Up: zoom' z0 '->' kzoom', In in view')
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=77'
call delay(50)
call readtrace
parse var trim.KEYZOOM . . . kzoom2
if kzoom2 <= kzoom then call writeln(log, 'FAILED Down: zoom' kzoom '->' kzoom2)
else call writeln(log, 'ok   Down: zoom' kzoom '->' kzoom2)
/* a short part (40 columns), Shift Space: just it */
call atin
call delay(50)
call readtrace
parse var view vin vout vtop vzoom
mx = wl + one.FIRSTCOL + (vin - vtop) % vzoom + 30
call mark mx, mx + 40, qy
call readtrace
parse var selected sa sb
p0 = n.PLAY
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=64 QUAL=1'
call delay(25)
call readtrace
parse var last.PLAY . . pa pb
if n.PLAY ~= p0 + 1 | pa ~= sa | pb ~= sb then call writeln(log, 'FAILED Shift Space: played' pa'..'pb', not the part' sa'..'sb)
else call writeln(log, 'ok   Shift Space: the part played,' pa'..'pb)
call deckkey 3
call delay(25)
/* Loop? on: the part played out plays again */
parse var last.LOOPITEM . . . tx ix iy
call pick tx ix iy
call readtrace
l0 = n.LOOPED
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=64 QUAL=1'
do i = 1 to 40 while n.LOOPED < l0 + 2
	call delay(10)
	call readtrace
end
parse var last.LOOPED . . la lb
if word(last.LOOP, 2) ~= 1 then call writeln(log, 'FAILED Loop? on: the trace says "'last.LOOP'"')
else if n.LOOPED < l0 + 2 | la ~= sa | lb ~= sb then call writeln(log, 'FAILED Loop: looped' n.LOOPED - l0 'times, last' la'..'lb)
else call writeln(log, 'ok   Loop: the part played again and again (' || n.LOOPED - l0 'times),' la'..'lb)
call deckkey 3
call delay(25)
call pick tx ix iy
call readtrace
if word(last.LOOP, 2) ~= 0 then call writeln(log, 'FAILED Loop? off: the trace says "'last.LOOP'"')

call writeln(log, 'O: Out''s line in the overview dragged 20 points left')
parse var box.overview ol ot ow oh
parse var read frames .
ocols = ow - 2 * one.FIRSTCOL
ozoom = (frames + ocols - 1) % ocols
/* Out at the end: its line (no flag there) on the overview's last
   column, which the trace gives */
ox = ol + one.OVERLAST
oy = ot + oh % 2
k = 'SNAPPED-OUT'
o0 = n.k
address command 'DEV:Test/InputPoke WIN=WaveSmith X='ox' Y='oy' CLICKS=0 STEPS=4'
call delay(25)
address command 'DEV:Test/InputPoke WIN=WaveSmith X='ox' Y='oy' SELECT=1'
address command 'DEV:Test/InputPoke WIN=WaveSmith X='ox - 20' Y='oy' SELECT=2 STEPS=8'
address command 'DEV:Test/InputPoke WIN=WaveSmith X='ox - 20' Y='oy' SELECT=0 STEPS=1'
call delay(100)
call readtrace
parse var trim.k kin kout ktop kzoom
if n.k ~= o0 + 1 then call writeln(log, 'FAILED Out''s line in the overview: no snapped Out (' || n.k - o0 'lines)')
else if frames - kout < 10 * ozoom | frames - kout > 30 * ozoom then
	call writeln(log, 'FAILED Out''s line: Out at' kout', not some 20 overview columns (' || ozoom 'frames each) before' frames)
else call writeln(log, 'ok   Out''s line in the overview: Out' frames '->' kout)
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-o.ppm'

call writeln(log, 'U: Undo and Redo (Right-Amiga Z and Y)')
/* the history began with the suggestion shown (the file read again
   after E's save); O's drag is its one step since */
parse var trim.PLAN pin pout .
ku = 'UNDONE'
kr = 'REDONE'
u0 = n.ku
r0 = n.kr
/* the emulator's keymap is German: Z and Y swap places -- Z is the raw
   key 21, Y 49 */
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=21 QUAL=128'
call delay(50)
call readtrace
parse var trim.ku uin uout upieces
if n.ku ~= u0 + 1 then call writeln(log, 'FAILED Undo: no undone line (' || n.ku - u0 ')')
else if uin ~= pin | uout ~= pout then call writeln(log, 'FAILED Undo: In and Out' uin uout', not the suggestion''s' pin pout)
else if one.DIRTY ~= 0 then call writeln(log, 'FAILED Undo back to the opened state: still marked changed')
else call writeln(log, 'ok   Undo: In and Out the suggestion''s again (' || uin uout'), the * gone')
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=49 QUAL=128'
call delay(50)
call readtrace
parse var trim.kr rin rout rpieces
if n.kr ~= r0 + 1 then call writeln(log, 'FAILED Redo: no redone line (' || n.kr - r0 ')')
else if rout ~= kout then call writeln(log, 'FAILED Redo: Out at' rout', not O''s' kout)
else if one.DIRTY ~= 1 then call writeln(log, 'FAILED Redo: not marked changed')
else call writeln(log, 'ok   Redo: Out at O''s' rout' again, the * back')
/* and undone again: G begins from the file as opened, as it did */
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=21 QUAL=128'
call delay(50)
call readtrace
if n.ku ~= u0 + 2 | one.DIRTY ~= 0 then call writeln(log, 'FAILED the second Undo: dirty' one.DIRTY', undone' n.ku - u0 'times')
else call writeln(log, 'ok   undone again: nothing changed')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-u.ppm'

call writeln(log, 'L: a part cut out, the parts joined')
/* the whole file in view, a part of it dragged over */
call click box.lensall
call delay(50)
call readtrace
parse var box.wave wl wt ww wh
ky = wt + wh % 2
call mark wl + one.FIRSTCOL + 200, wl + one.FIRSTCOL + 260, ky
call readtrace
parse var selected sa sb
kc = 'CUT'
c0 = n.kc
/* Right-Amiga X, Cut out: the question; Return, Join (the leftmost) */
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=50 QUAL=128'
call delay(100)
address command 'DEV:Test/InputPoke KEY=68'
call delay(150)
call readtrace
parse var trim.kc ca cb cj
parse var last.TIMELINE . . tp tf .
if n.kc ~= c0 + 1 then call writeln(log, 'FAILED Cut out: no cut line (' || n.kc - c0 ')')
else if cj ~= 1 then call writeln(log, 'FAILED Cut out: not joined (answer' answer')')
else if abs(ca - sa) > 4000 | abs(cb - sb) > 4000 then call writeln(log, 'FAILED Cut out' ca'..'cb', not the part marked' sa'..'sb)
else if tp ~= 2 | tf ~= frames - (cb - ca) then call writeln(log, 'FAILED the timeline after the cut:' tp 'pieces,' tf 'frames, not 2,' frames - (cb - ca))
else if one.DIRTY ~= 1 then call writeln(log, 'FAILED Cut out: not marked changed')
else call writeln(log, 'ok   cut out' ca'..'cb', joined: 2 pieces,' tf 'frames')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-l.ppm'
/* Undo: the whole file again; Redo: the cut again */
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=21 QUAL=128'
call delay(80)
call readtrace
parse var last.TIMELINE . . up uf .
if up ~= 1 | uf ~= frames then call writeln(log, 'FAILED Undo of the cut:' up 'pieces,' uf 'frames, not the whole file''s' frames)
else call writeln(log, 'ok   Undo: the whole file again')
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=49 QUAL=128'
call delay(80)
call readtrace
parse var last.TIMELINE . . rp rf .
if rp ~= 2 | rf ~= tf then call writeln(log, 'FAILED Redo of the cut:' rp 'pieces,' rf 'frames, not 2,' tf)
else call writeln(log, 'ok   Redo: cut again')

call writeln(log, 'M: another part cut out, silence in its place')
call mark wl + one.FIRSTCOL + 400, wl + one.FIRSTCOL + 430, ky
call readtrace
/* Right-Amiga X, then S: the question's Silence (its underscored key) */
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=50 QUAL=128'
call delay(100)
address command 'DEV:Test/InputPoke KEY=33'
call delay(150)
call readtrace
parse var trim.kc ma mb mj
parse var last.TIMELINE . . mp mf .
if n.kc ~= c0 + 2 then call writeln(log, 'FAILED Cut out to silence: no cut line (' || n.kc - c0 'in all)')
else if mj ~= 0 then call writeln(log, 'FAILED Cut out to silence: joined (answer' answer')')
else if mp ~= 4 | mf ~= tf then call writeln(log, 'FAILED the timeline after the silence:' mp 'pieces,' mf 'frames, not 4,' tf)
else call writeln(log, 'ok   cut out' ma'..'mb', silence in its place: 4 pieces, the length kept')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-m.ppm'

call writeln(log, 'V: Split at the cursor')
/* the play marker where a click on the sound puts it (stopped) */
call click wl + one.FIRSTCOL + 300 ky 0 0
call delay(50)
call readtrace
vat = seekat
ks = 'SPLIT'
s0 = n.ks
p0 = n.PARTNAME
parse var last.SPLITITEM . . . stx six siy
address command 'DEV:Test/InputPoke X='stx' Y=4 MENU=1'
call delay(15)
address command 'DEV:Test/InputPoke X='six' Y='siy' MENU=2'
call delay(15)
address command 'DEV:Test/InputPoke MENU=0'
/* two files written, the save slowed: some seconds */
do i = 1 to 80 while n.ks = s0
	call delay(25)
	call readtrace
end
parse var trim.ks sin sat sout serr
if n.ks ~= s0 + 1 then call writeln(log, 'FAILED Split: no split line (menu at' last.SPLITITEM')')
else if serr ~= 0 then call writeln(log, 'FAILED Split: error' serr)
else if abs(sat - vat) > 4000 then call writeln(log, 'FAILED Split at' sat', not at the play marker' vat)
else if one.DIRTY ~= 1 then call writeln(log, 'FAILED Split: the doc no longer marked changed')
else call writeln(log, 'ok   split at' sat' (the marker at' vat'):' sin'..'sat 'and' sat'..'sout', the doc as it was')
/* the status line named each part as it was written, not Trim.wav */
if n.PARTNAME ~= p0 + 2 | one.PARTNAME ~= 'Trim-2.wav' then call writeln(log, 'FAILED Split: the parts named' n.PARTNAME - p0 'times, the last "'one.PARTNAME'"')
else call writeln(log, 'ok   Split: the status line named Trim-1.wav, then Trim-2.wav')
if ~exists('DEV:Test/Edit/Trim-1.wav.info') | ~exists('DEV:Test/Edit/Trim-2.wav.info') | word(last.ICON, 2) ~= 1 then
	call writeln(log, 'FAILED Split: the parts without icons (last "'last.ICON'")')
else call writeln(log, 'ok   Split: both parts with an icon')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-v.ppm'

call writeln(log, 'R: the weight bar under the overview dragged 30 pixels down')
parse var box.overview rol rot row roh
parse var box.wave . rwt . rwh
/* the bar in the gap from the overview's bottom to the waveform's top */
bary = (rot + roh + rwt) % 2
barx = rol + row % 2
k = 'OVERVIEW'
r0 = n.k
address command 'DEV:Test/InputPoke WIN=WaveSmith X='barx' Y='bary' CLICKS=0 STEPS=4'
call delay(25)
address command 'DEV:Test/InputPoke WIN=WaveSmith X='barx' Y='bary' SELECT=1'
address command 'DEV:Test/InputPoke WIN=WaveSmith X='barx' Y='bary + 30' SELECT=2 STEPS=8'
address command 'DEV:Test/InputPoke WIN=WaveSmith X='barx' Y='bary + 30' SELECT=0 STEPS=1'
/* the trace gives both boxes anew at the tick after the heights changed */
do i = 1 to 30 while n.k = r0
	call delay(10)
	call readtrace
end
parse var box.overview . . . roh2
parse var box.wave . rwt2 . rwh2
if n.k = r0 then call writeln(log, 'FAILED the weight bar at' barx','bary': the overview kept its height' roh)
else if abs(roh2 - roh - 30) > 4 then call writeln(log, 'FAILED the weight bar dragged 30 down: the overview' roh '->' roh2)
else if abs((roh2 + rwh2) - (roh + rwh)) > 2 then call writeln(log, 'FAILED the weight bar: overview + waveform' roh + rwh '->' roh2 + rwh2)
else if rwt2 - rwt ~= roh2 - roh then call writeln(log, 'FAILED the weight bar: the waveform''s top' rwt '->' rwt2', the overview' roh '->' roh2)
else call writeln(log, 'ok   the weight bar dragged 30 down: overview' roh '->' roh2', waveform' rwh '->' rwh2)
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-r.ppm'
/* back up where it was: the overview at its least height again, which
   the bar cannot take it under */
parse var box.overview . rot2 . roh2
bary = (rot2 + roh2 + rwt2) % 2
r0 = n.k
address command 'DEV:Test/InputPoke WIN=WaveSmith X='barx' Y='bary' CLICKS=0 STEPS=4'
call delay(25)
address command 'DEV:Test/InputPoke WIN=WaveSmith X='barx' Y='bary' SELECT=1'
address command 'DEV:Test/InputPoke WIN=WaveSmith X='barx' Y='bary - 40' SELECT=2 STEPS=8'
address command 'DEV:Test/InputPoke WIN=WaveSmith X='barx' Y='bary - 40' SELECT=0 STEPS=1'
do i = 1 to 30 while n.k = r0
	call delay(10)
	call readtrace
end
parse var box.overview . . . roh3
parse var box.wave . rwt3 . rwh3
if roh3 ~= roh | rwh3 ~= rwh | rwt3 ~= rwt then
	call writeln(log, 'FAILED the weight bar dragged 40 up: overview' roh3', waveform' rwt3 rwh3', not' roh', 'rwt rwh)
else call writeln(log, 'ok   the weight bar dragged 40 up: the overview at its least,' roh', the waveform' rwh 'again')

call writeln(log, 'Y: Save as TrimAs.wav')
k = 'SAVEDAS'
y0 = n.k
p0 = n.PLAN
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=32 QUAL=128'
call delay(150)
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-y.ppm'
/* ASL's file field is the active one: emptied (Del and Backspace past
   "Trim.wav"), the name typed (the German keymap's raw keys; Shift for
   the capitals), Return */
do ki = 1 to 10
	address command 'DEV:Test/InputPoke KEY=70'
	address command 'DEV:Test/InputPoke KEY=65'
end
address command 'DEV:Test/InputPoke KEY=20 QUAL=1'
do ki = 1 to 3
	address command 'DEV:Test/InputPoke KEY='word('19 23 55', ki)
end
address command 'DEV:Test/InputPoke KEY=32 QUAL=1'
do ki = 1 to 5
	address command 'DEV:Test/InputPoke KEY='word('33 57 17 32 52', ki)
end
address command 'DEV:Test/InputPoke KEY=68'
/* written (slowed: some seconds), then read anew */
do i = 1 to 120 while n.k = y0 | n.PLAN = p0
	call delay(25)
	call readtrace
end
parse var trim.SAVEDAS . . yerr
if n.k ~= y0 + 1 then call writeln(log, 'FAILED Save as: no savedas line')
else if yerr ~= 0 then call writeln(log, 'FAILED Save as: error' yerr)
else if last.SAVEDASNAME ~= 'TrimAs.wav' then call writeln(log, 'FAILED Save as: written as "'last.SAVEDASNAME'"')
else if ~exists('DEV:Test/Edit/TrimAs.wav') | ~exists('DEV:Test/Edit/TrimAs.prl') | ~exists('DEV:Test/Edit/TrimAs.wav.info') then
	call writeln(log, 'FAILED Save as: TrimAs.wav, its map or its icon missing')
else if n.PLAN ~= p0 + 1 then call writeln(log, 'FAILED Save as: TrimAs.wav not read anew (' || n.PLAN - p0 'plans)')
else if one.DIRTY ~= 0 then call writeln(log, 'FAILED Save as: the doc still marked changed')
else call writeln(log, 'ok   Save as: TrimAs.wav written with map and icon, read anew, nothing changed')
cur = 'TrimAs'

call writeln(log, 'T: Settings: snap off and on, a backup kept, saved')
/* the control first: In's line dragged on with the snap on moves off
   the frame let go at (a few distances tried: one may let go on a
   crossing); undone, the same drag with it off stays on that frame */
do di = 1 to 4
	dd = word('20 23 26 29', di)
	call drag_in dd
	parse var last.SNAP . . sfrom1 sto1
	address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=21 QUAL=128'
	call delay(50)
	if sfrom1 ~= sto1 then leave
end
parse var last.SNAPITEM . . . tx ix iy
call pick tx ix iy
call readtrace
if word(space(trim.PREFS, 1), 1) ~= 0 then call writeln(log, 'FAILED Snap to zero crossings? off: the settings say' space(trim.PREFS, 1))
call drag_in dd
parse var last.SNAP . . sfrom2 sto2
if sfrom1 = sto1 then call writeln(log, 'FAILED snap off untested: the control (snap on) let go on a crossing,' sfrom1)
else if sfrom2 ~= sfrom1 then call writeln(log, 'FAILED snap off: let go at' sfrom2', the control at' sfrom1)
else if sfrom2 ~= sto2 then call writeln(log, 'FAILED snap off: In moved from' sfrom2 'to' sto2)
else call writeln(log, 'ok   snap off: In let go at' sfrom2', not moved (on, it went to' sto1')')
call pick tx ix iy
parse var last.BACKUPITEM . . . tx ix iy
call pick tx ix iy
call readtrace
if space(trim.PREFS, 1) ~= '1 500 2000 1' then call writeln(log, 'FAILED snap on, backup on: the settings say' space(trim.PREFS, 1))
else call writeln(log, 'ok   snap on again, Keep a backup? on')
/* Save settings: ENVARC: (none was read), the weight bar's place in it */
parse var last.SAVEPREFSITEM . . . tx ix iy
call pick tx ix iy
call delay(50)
call readtrace
parse var box.overview . . . roh
parse var box.wave . . . rwh
share = (roh * 100 + (roh + rwh) % 2) % (roh + rwh)
if share < 5 then share = 5
if share > 50 then share = 50
pf = ''
if open(pr, 'ENVARC:WaveSmith.prefs', 'R') then do
	do while ~eof(pr)
		pf = pf readln(pr)
	end
	call close(pr)
end
pf = space(pf, 1)
if word(last.PREFSSAVED, 2) ~= 0 then call writeln(log, 'FAILED Save settings: error' last.PREFSSAVED)
else if pos('SNAP=YES', pf) = 0 | pos('BACKUP=YES', pf) = 0 | pos('ICONS=YES', pf) = 0 | pos('LEAD=500', pf) = 0 | pos('OVERVIEW='share, pf) = 0 then
	call writeln(log, 'FAILED Save settings: ENVARC:WaveSmith.prefs holds "'pf'" (the share' share')')
else if ~exists('ENV:WaveSmith.prefs') then call writeln(log, 'FAILED Save settings: nothing in ENV:')
else call writeln(log, 'ok   Save settings: ENVARC: and ENV:, "'pf'"')
/* what G's save keeps as the backup */
parse value statef('DEV:Test/Edit/'cur'.wav') with . baksize .
parse value statef('DEV:Test/Edit/'cur'.prl') with . prlsize .

call writeln(log, 'N: the Trim window (Edit / Trim..., Right-Amiga T)')
k = 'TRIMWIN'
w0 = n.k
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=20 QUAL=128'
/* read once after a wait: the trace is long by now, and a read of it
   takes seconds on the emulated 68030 */
call delay(50)
call readtrace
if n.k = w0 then do
	call delay(50)
	call readtrace
end
miss = ''
do gi = 1 to 12
	g = upper(word('trimin triminback triminon triminlisten triminfade triminshow trimout trimoutback trimouton trimoutlisten trimoutfade trimoutshow', gi))
	if box.g = '' then miss = miss g
end
if n.k = w0 then call writeln(log, 'FAILED Right-Amiga T: no Trim window in the trace')
else if miss ~= '' then call writeln(log, 'FAILED the Trim window: not traced:' miss)
else call writeln(log, 'ok   the Trim window open,' last.k', its twelve gadgets traced')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-n.ppm'
/* Show In: the view at In (the view line says where In is) */
call trimshow
parse var view in1 . . z1
if ~datatype(in1, 'W') | ~datatype(z1, 'W') then call writeln(log, 'FAILED Show In: no view line')
else call writeln(log, 'ok   Show In: the view at In,' view)
/* In's arrow back: a step of the zoom */
call clicktrim box.triminback
call delay(25)
call trimshow
parse var view in2 . . .
if in2 ~= in1 - z1 then call writeln(log, 'FAILED In''s arrow back: In from' in1 'to' in2', not' z1 'back')
else call writeln(log, 'ok   In''s arrow back: In' z1 'frames back, at' in2)
/* undone: where it was, the doc as before (G wants its own change) */
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=21 QUAL=128'
call delay(50)
call trimshow
parse var view in3 . . .
if in3 ~= in1 then call writeln(log, 'FAILED Undo after the arrow: In at' in3', not' in1)
else call writeln(log, 'ok   Undo: In back at' in3)
/* its close gadget: closed, said */
address command 'DEV:Test/InputPoke WIN=Trim X=8 Y=5 STEPS=4'
call delay(30)
call readtrace
if last.k ~= 'closed' then call writeln(log, 'FAILED the Trim window''s close gadget: "'last.k'"')
else call writeln(log, 'ok   the Trim window closed by its close gadget')

call writeln(log, 'F: the handle flags')
/* Out's flag: Out picked (the trace's "pick out 1") and in view (the
   views counted through kv: n.VIEW would read the variable view) */
k = 'PICK'
kv = 'VIEW'
v0 = n.kv
call click box.flagout
call delay(40)
call readtrace
parse var view fin fout ftop fzoom
if last.k ~= 'out 1' then call writeln(log, 'FAILED Out''s flag: picked "'last.k'", not Out')
else if n.kv ~= v0 + 1 | fout < ftop | fout > ftop + cols * fzoom then
	call writeln(log, 'FAILED Out''s flag: the view' ftop 'x' fzoom 'does not show Out' fout)
else call writeln(log, 'ok   Out''s flag: Out picked, in view')
/* In's flag: In again */
v0 = n.kv
call click box.flagin
call delay(40)
call readtrace
parse var view fin fout ftop fzoom
if last.k ~= 'out 0' then call writeln(log, 'FAILED In''s flag: picked "'last.k'", not In')
else if n.kv ~= v0 + 1 | fin < ftop | fin > ftop + cols * fzoom then
	call writeln(log, 'FAILED In''s flag: the view' ftop 'x' fzoom 'does not show In' fin)
else call writeln(log, 'ok   In''s flag: In picked, in view')
/* a double-click on In's flag on the sound (right of its line, under
   the ruler, as G grabs it): the Trim window */
k = 'TRIMWIN'
w0 = n.k
parse var box.wave wl wt ww wh
px = wl + one.FIRSTCOL + (fin - ftop) % fzoom + 8
/* the pointer there first and what it is over read (the waveforms'
   bubble text): a double-click that lands elsewhere says where */
address command 'DEV:Test/InputPoke WIN=WaveSmith X='px' Y='wt + 17' CLICKS=0 STEPS=4'
call delay(25)
call readtrace
hoverwas = helptext
address command 'DEV:Test/InputPoke WIN=WaveSmith X='px' Y='wt + 17' CLICKS=2'
call delay(50)
call readtrace
if n.k = w0 then call writeln(log, 'FAILED a double-click on In''s flag at' px','wt + 17': no Trim window (over "'hoverwas'", then "'helptext'")')
else call writeln(log, 'ok   a double-click on In''s flag: the Trim window')
address command 'DEV:Test/InputPoke WIN=Trim X=8 Y=5 STEPS=4'
call delay(30)
call readtrace
if last.k ~= 'closed' then call writeln(log, 'FAILED the Trim window not closed again: "'last.k'"')

call writeln(log, 'G: a change, then Quit asks')
call atin
call delay(50)
call readtrace
parse var view vin vout vtop vzoom
/* by its flag this time, not its line: the flag right of In, on the
   lanes' top under the ruler (Topaz 8: 11 lines, the flag 10) */
k = 'SNAPPED-IN'
g0 = n.k
parse var box.wave wl wt ww wh
px = wl + one.FIRSTCOL + (vin - vtop) % vzoom + 8
fy = wt + 17
address command 'DEV:Test/InputPoke WIN=WaveSmith X='px' Y='fy' CLICKS=0 STEPS=4'
call delay(25)
address command 'DEV:Test/InputPoke WIN=WaveSmith X='px' Y='fy' SELECT=1'
address command 'DEV:Test/InputPoke WIN=WaveSmith X='px + 20' Y='fy' SELECT=2 STEPS=8'
address command 'DEV:Test/InputPoke WIN=WaveSmith X='px + 20' Y='fy' SELECT=0 STEPS=1'
call delay(100)
call readtrace
k = 'SNAPPED-IN'
if n.k ~= g0 + 1 then call writeln(log, 'FAILED In dragged by its flag at' px','fy': no snapped In (' || n.k - g0 'lines; over "'helptext'")')
else if one.DIRTY ~= 1 then call writeln(log, 'FAILED In dragged by its flag, the file not marked changed (dirty' one.DIRTY')')
else call writeln(log, 'ok   In dragged by its flag: snapped, the file marked changed')
a0 = n.ASKED
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=16 QUAL=128'
call delay(100)
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-g.ppm'
/* the question is a window of WaveSmith's own, centred on its window:
   Esc answers the rightmost, Cancel; it must sit over the main window */
address command 'DEV:Test/ScreenInfo >T:edit-ask.box'
call askedwhere
address command 'DEV:Test/InputPoke KEY=69'
call delay(50)
call readtrace
/* where it opened (the trace, the moment WM_OPEN returned): where it
   stood -- a window that opens elsewhere and moves blinks there first */
parse var trim.ASKOPENED ox oy ow oh
parse var askbox bx by bw bh
/* numbers compared as numbers: the trace pads them */
if ox ~= bx | oy ~= by | ow ~= bw | oh ~= bh then call writeln(log, 'FAILED the question opened at' space(trim.ASKOPENED, 1)', then stood at' askbox)
else call writeln(log, 'ok   the question opened where it stood:' askbox)
address command 'Status >NIL: COMMAND=DEV:sasc/WaveSmith'
running = (rc = 0)
if n.ASKED ~= a0 + 1 then call writeln(log, 'FAILED Quit with a change asked' n.ASKED - a0 'times, not once')
else if answer ~= 0 then call writeln(log, 'FAILED the requester answered' answer', not Cancel (0)')
else if ~running then call writeln(log, 'FAILED Cancel, and WaveSmith quit all the same')
else if one.DIRTY ~= 1 then call writeln(log, 'FAILED Cancel, and the change is gone')
else call writeln(log, 'ok   Quit asked; Cancel: still running, still changed')
s0 = n.SAVED
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=16 QUAL=128'
call delay(100)
/* Return: the leftmost, Save */
address command 'DEV:Test/InputPoke KEY=68'
call delay(300)
call readtrace
address command 'Status >NIL: COMMAND=DEV:sasc/WaveSmith'
running = (rc = 0)
if n.ASKED ~= a0 + 2 then call writeln(log, 'FAILED Quit asked' n.ASKED - a0 - 1 'times the second time, not once')
else if answer ~= 1 then call writeln(log, 'FAILED the requester answered' answer', not Save (1)')
else if n.SAVED ~= s0 + 1 then call writeln(log, 'FAILED Save in the requester saved nothing')
else if running then call writeln(log, 'FAILED saved, but WaveSmith did not quit')
else call writeln(log, 'ok   asked again, Save: saved and quit')
/* T's Keep a backup?: the file saved over kept, its map too */
parse value statef('DEV:Test/Edit/'cur'.wav.bak') with . bs .
parse value statef('DEV:Test/Edit/'cur'.prl.bak') with . ps .
if bs ~= baksize | ps ~= prlsize then
	call writeln(log, 'FAILED the backup:' cur'.wav.bak' bs 'bytes,' cur'.prl.bak' ps', not' baksize 'and' prlsize)
else call writeln(log, 'ok   the backup:' cur'.wav.bak' bs 'bytes,' cur'.prl.bak' ps', the files saved over')
if running then do
	call writeln(log, 'F: Quit')
	address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=16 QUAL=128'
	call delay(100)
end
exit 0

syntax:
	call writeln(log, 'FAILED ARexx error' rc '(' || errortext(rc) || ') in line' sigl)
	exit 20

/* Settings, Lead and tail... opened, its lead's field clicked, emptied,
   the raw keys typed (digits: 1-9 are 1-9, 0 is 10),
   then Return in the field when asked; the window's boxes in box. */
lead_typed: procedure expose box. trim. one. last. view read n. helptext pausedat playat seekat answer selected menuat beats savederr
	parse arg keys, ret
	parse var last.MARGINSITEM . . . tx ix iy
	call pick tx ix iy
	call delay(50)
	call readtrace
	parse var box.MARGINSLEAD ll lt lw lh
	address command 'DEV:Test/InputPoke WIN=Lead X='ll + lw % 2' Y='lt + lh % 2' STEPS=4'
	call delay(25)
	/* emptied wherever the click put the cursor: Del and Backspace past
	   its five digits (Right-Amiga X leaves an integer.gadget as it is) */
	do i = 1 to 6
		address command 'DEV:Test/InputPoke KEY=70'
		address command 'DEV:Test/InputPoke KEY=65'
	end
	do i = 1 to words(keys)
		address command 'DEV:Test/InputPoke KEY='word(keys, i)
	end
	if ret = 'RETURN' then address command 'DEV:Test/InputPoke KEY=68'
	call delay(25)
	return

/* At In, then In's line dragged so many pixels on (the trace read after) */
drag_in: procedure expose box. trim. one. last. view read n. helptext pausedat playat seekat answer selected menuat beats savederr
	parse arg dx
	call atin
	call delay(50)
	call readtrace
	parse var view vin vout vtop vzoom
	parse var box.wave wl wt ww wh
	px = wl + one.FIRSTCOL + (vin - vtop) % vzoom
	py = wt + (wh * 2) % 3
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='px' Y='py' CLICKS=0 STEPS=4'
	call delay(25)
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='px' Y='py' SELECT=1'
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='px + dx' Y='py' SELECT=2 STEPS=8'
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='px + dx' Y='py' SELECT=0 STEPS=1'
	call delay(100)
	call readtrace
	return

/* a menu item picked: the title at tx on the bar, the item at ix, iy */
pick: procedure
	parse arg tx ix iy
	address command 'DEV:Test/InputPoke X='tx' Y=4 MENU=1'
	call delay(15)
	address command 'DEV:Test/InputPoke X='ix' Y='iy' MENU=2'
	call delay(15)
	address command 'DEV:Test/InputPoke MENU=0'
	call delay(25)
	return

/* a drag over the sound from x1 to x2 at y: a part marked */
mark: procedure
	parse arg x1, x2, y
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='x1' Y='y' CLICKS=0 STEPS=4'
	call delay(25)
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='x1' Y='y' SELECT=1'
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='x2' Y='y' SELECT=2 STEPS=8'
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='x2' Y='y' SELECT=0 STEPS=1'
	call delay(50)
	return

/* where the question opened: ScreenInfo names the active window -- the
   question, titled WaveSmith alone -- and its box, inside the main
   window's (box.window, the trace's) and centred on it */
askedwhere: procedure expose box. log askbox
	if ~open(ab, 'T:edit-ask.box', 'R') then do
		call writeln(log, 'FAILED no box for the question')
		return
	end
	line = ''
	do while ~eof(ab) & left(line, 14) ~= 'active window:'
		line = readln(ab)
	end
	call close(ab)
	parse var line 'active window: ' title ', ' aw 'x' ah ', at least ' . ', at ' ax ',' ay
	ay = strip(ay)
	askbox = ax ay aw ah
	if title ~= 'WaveSmith' then call writeln(log, 'FAILED the active window is "'title'", not the question')
	parse var box.window mx my mw mh
	if ~datatype(ax, 'W') | ~datatype(aw, 'W') then call writeln(log, 'FAILED the question''s box unread: "'line'"')
	else if ax < mx | ay < my | ax + aw > mx + mw | ay + ah > my + mh then
		call writeln(log, 'FAILED the question at' ax','ay aw'x'ah', not over the window' mx','my mw'x'mh)
	else if abs((ax + aw % 2) - (mx + mw % 2)) > 2 | abs((ay + ah % 2) - (my + mh % 2)) > 2 then
		call writeln(log, 'FAILED the question at' ax','ay aw'x'ah', not centred on the window' mx','my mw'x'mh)
	else call writeln(log, 'ok   the question centred on the window:' ax','ay aw'x'ah)
	return

/* the wheel's checks: over In's column in the waveform a notch away
   (rawkey $7A, NewMouse's up: V47 makes it the wheel) halves the zoom and
   leaves In in its column; a notch back ($7B) doubles it again; Shift
   scrolls forward without zooming; a notch over the overview at Out's
   column brings Out into the waveform */
wheel: procedure expose box. trim. one. view read log n. helptext
	parse var trim.SHOWN sin sout stop szoom
	parse var box.wave wl wt ww wh
	parse var box.overview ol ot ow oh
	if szoom = '' then do
		call writeln(log, 'FAILED no shown line in the trace')
		return
	end
	col = (sin - stop) % szoom
	px = wl + one.FIRSTCOL + col
	py = wt + wh % 2
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='px' Y='py' CLICKS=0 STEPS=4'
	call delay(25)
	call notch 122, 0
	parse var trim.WHEEL win wout wtop wzoom
	call writeln(log, 'wheel in:' trim.WHEEL '(shown' trim.SHOWN')')
	if wzoom ~= (szoom + 1) % 2 then call writeln(log, 'FAILED a notch away did not halve the zoom:' wzoom)
	else if abs((sin - wtop) % wzoom - col) > 1 then call writeln(log, 'FAILED In left its column:' (sin - wtop) % wzoom 'not' col)
	else call writeln(log, 'ok   a notch away: zoom' wzoom', In still in column' col)
	call notch 123, 0
	parse var trim.WHEEL win wout wtop wzoom
	if wzoom ~= szoom then call writeln(log, 'FAILED a notch back did not restore the zoom:' wzoom 'not' szoom)
	else call writeln(log, 'ok   a notch back: zoom' wzoom)
	call notch 122, 0
	call notch 122, 0
	parse var trim.WHEEL win wout wtop1 wzoom1
	call notch 123, 1
	parse var trim.WHEEL win wout wtop2 wzoom2
	if wzoom2 ~= wzoom1 | wtop2 <= wtop1 then call writeln(log, 'FAILED Shift and the wheel: top' wtop1 '->' wtop2', zoom' wzoom1 '->' wzoom2)
	else call writeln(log, 'ok   Shift and a notch: top' wtop1 '->' wtop2 'at zoom' wzoom2)
	/* the overview: the whole file in its inner width */
	parse var read frames .
	ow = ow - 2 * one.FIRSTCOL
	ocol = sout % ((frames + ow - 1) % ow)
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='ol + one.FIRSTCOL + ocol' Y='ot + oh % 2' CLICKS=0 STEPS=4'
	call delay(25)
	call notch 122, 0
	parse var trim.WHEEL win wout wtop wzoom
	if sout < wtop | sout >= wtop + (ww - 2 * one.FIRSTCOL) * wzoom then call writeln(log, 'FAILED a notch over the overview at Out: view' wtop 'zoom' wzoom)
	else call writeln(log, 'ok   a notch over the overview at Out: view from' wtop 'zoom' wzoom)
	return

/* the bubbles: the pointer glided there (a hand's moves: window.class
   closes a bubble only after more than two), then left over 0.7 s */
bubbles: procedure expose box. trim. one. view read log n. helptext
	parse var trim.SHOWN sin sout stop szoom
	parse var box.wave wl wt ww wh
	px = wl + one.FIRSTCOL + (sin - stop) % szoom
	call glide px, wt + (wh * 2) % 3
	if helptext ~= 'In 0:03.600' then call writeln(log, 'FAILED over In the bubble said "'helptext'"')
	else call writeln(log, 'ok   over In: "'helptext'"')
	address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-h.ppm'
	call glide px + 100, wt + (wh * 2) % 3
	parse var helptext mm ':' ss '.' ms
	if ~datatype(mm, 'W') | ~datatype(ss, 'W') | ~datatype(ms, 'W') | length(ms) ~= 3 then
		call writeln(log, 'FAILED over the sound the bubble said "'helptext'"')
	else call writeln(log, 'ok   over the sound: "'helptext'"')
	call glide px + 100, wt + one.FIRSTCOL + 3
	if helptext ~= '-' then call writeln(log, 'FAILED over the ruler a bubble: "'helptext'"')
	else call writeln(log, 'ok   over the ruler: none')
	parse var box.trimbutton sl st sw sh
	call glide sl + sw % 2, st + sh % 2
	address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-h2.ppm'
	return

glide: procedure expose box. trim. one. view read n. helptext
	parse arg gx, gy
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='gx' Y='gy' CLICKS=0 STEPS=6'
	call delay(60)
	call readtrace
	return

/* a notch (rawkey code, Shift or not), the trace read after it */
notch: procedure expose box. trim. one. view read log n. helptext
	parse arg code, shift
	before = n.WHEEL
	if shift then address command 'DEV:Test/InputPoke WIN=WaveSmith KEY='code' QUAL=1'
	else address command 'DEV:Test/InputPoke WIN=WaveSmith KEY='code
	call delay(25)
	call readtrace
	if n.WHEEL ~= before + 1 then call writeln(log, 'FAILED key' code 'brought' n.WHEEL - before 'wheel lines, not one')
	return

/* a button's middle, the pointer gliding there first (a tooltip closes) */
click: procedure
	parse arg l t w h
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='l + w % 2' Y='t + h % 2' STEPS=4'
	return

/* a key of the deck -- rewind 0, play 1, forward 2, stop 3, pause 4 -- a
   fifth of its box each (tapedeck.gadget's five keys side by side) */
deckkey: procedure expose box.
	parse arg k
	parse var box.DECK l t w h
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='l + (w * (2 * k + 1)) % 10' Y='t + h % 2' STEPS=4'
	return

/* View / At In by its place in the menu (the trace's atinitem) */
atin: procedure expose last.
	parse var last.ATINITEM . . . tx ix iy
	call pick tx ix iy
	return

/* a gadget of the Trim window (its title: "Trim") */
clicktrim: procedure
	parse arg l t w h
	address command 'DEV:Test/InputPoke WIN=Trim X='l + w % 2' Y='t + h % 2' STEPS=4'
	return

/* the Trim window's Show In, waited for its view line */
trimshow: procedure expose box. trim. one. last. view read n. helptext pausedat playat seekat answer selected menuat beats savederr
	kv = 'VIEW'
	v0 = n.kv
	call clicktrim box.triminshow
	call delay(30)
	call readtrace
	if n.kv = v0 then do
		call delay(50)
		call readtrace
	end
	return

/* the trace: the boxes by name, the handle lines' last four numbers
   by name (in out top zoom), the last view. The names upper-cased: a
   tail written box.wave is looked up as WAVE */
readtrace: procedure expose box. trim. one. last. view read n. helptext pausedat playat seekat answer selected menuat beats savederr
	n. = 0
	if ~open(tr, 'DEV:Test/edit.trace', 'R') then return
	do while ~eof(tr)
		line = readln(tr)
		parse var line what rest
		what = upper(what)
		n.what = n.what + 1
		last.what = rest
		if what = 'HELP' then helptext = strip(rest)
		if what = 'READ' then parse var rest . . read
		/* a save keeps the doc: the file's length now the one saved */
		if what = 'KEPTSAVE' then parse var rest . . . read
		if what = 'PAUSED' then pausedat = word(rest, 2)
		if what = 'PLAY' then playat = word(rest, 3)
		if what = 'SEEK' then seekat = word(rest, 2)
		if what = 'BEATS' then beats = subword(rest, 3, 2)
		if what = 'SAVED' then savederr = word(rest, 6)
		if what = 'ASKED' then answer = word(rest, 2)
		if what = 'SELECTED' then selected = subword(rest, 3, 2)
		if what = 'MENUITEM' then menuat = subword(rest, 4, 3)
		if words(rest) = 1 then one.what = strip(rest)
		else if words(rest) = 6 then do
			/* undone / redone in out pieces */
			parse var rest . . . a b c
			trim.what = a b c
		end
		else if words(rest) = 8 then do
			parse var rest . . . . a b c d
			trim.what = a b c d
			if what = 'VIEW' then view = a b c d
		end
		else if words(rest) = 4 & datatype(word(rest, 1), 'W') then box.what = rest
	end
	call close(tr)
	return
