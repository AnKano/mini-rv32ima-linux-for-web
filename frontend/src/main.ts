import { instantiateEmulator } from "./emulator";
import { instantiateTerminal } from "./terminal";

import './popup-button';

const terminalContainer = document.getElementById('terminal');
if (!terminalContainer) throw 'page structure corrupted!';

const emulator = instantiateEmulator();
instantiateTerminal(terminalContainer, emulator);