import { Emulator } from './emulator';
import { Terminal } from '@xterm/xterm';
import { FitAddon } from '@xterm/addon-fit';

export function instantiateTerminal(container: HTMLElement, emulator: Emulator) {
    const term = new Terminal({ fontFamily: 'UbuntuMono-Regular, monospace' });
    term.open(container);

    const fitAddon = new FitAddon();
    term.loadAddon(fitAddon);
    term.open(container);
    fitAddon.fit();

    setInterval(function () {
        emulator.outputQueue.forEach((text) => term.write(text));
        emulator.outputQueue = [];
    }, 1);

    term.onKey((arg1) => {
        const { key } = arg1;

        if (key.charCodeAt(0) == 13) {
            emulator.mrv32worker.postMessage({ event: 'input', payload: { data: 13 } });
            return;
        }

        key.split('').forEach(element => {
            emulator.mrv32worker.postMessage({ event: 'input', payload: { data: element.charCodeAt(0) } });
        });
    });
}