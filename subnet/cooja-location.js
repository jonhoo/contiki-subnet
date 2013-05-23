while(1) {
    YIELD_THEN_WAIT_UNTIL(msg.equals("acquiring position..."));

    var x = mote.getInterfaces().getPosition().getXCoordinate();
    var y = mote.getInterfaces().getPosition().getYCoordinate();

    x = x | 0
    y = y | 0

    write(mote, x + "\n" + y);
}
