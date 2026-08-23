from pathlib import Path

root = Path(__file__).resolve().parents[1]
popup = (root / "src/Screens/MixScreen/MatrixPopUpPicker.cpp").read_text()
session = (root / "src/DjSession/DjSession.cpp").read_text()
wireless = (root / "src/Wireless/WirelessBringup.cpp").read_text()

assert "requestPairing(DJ_ORIGIN_LOCAL_UI)" in popup
assert "Pair remote" in popup
assert "copyPairingStatus" in popup
assert "command.origin == DJ_ORIGIN_PHYSICAL || command.origin == DJ_ORIGIN_LOCAL_UI" in session

setup = wireless[wireless.index("void handleSetupWifi()"):wireless.index("void handlePair()")]
assert "if(!sameOrigin())" in setup
assert "origin_rejected" in setup
