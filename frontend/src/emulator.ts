export class Emulator {
    public outputQueue: string[] = [];
    mrv32worker: Worker;

    constructor() {
        this.mrv32worker = new Worker('mini-rv32ima-glue.js');
        this.initWorkerEvents();
    }

    private initWorkerEvents() {
        this.mrv32worker.addEventListener('message', (e: any) => {
            const data = e.data;
            if (data.event == 'ready') {
                this.mrv32worker.postMessage({ event: 'initialize', payload: null });
            } else if (data.event == 'initializationEnd') {
                this.mrv32worker.postMessage({ event: 'performOneCycle', payload: null });
            } else if (data.event == 'oneCycleEnd') {
                if (data.payload.data.length > 0) {
                    this.outputQueue.push(data.payload.data);
                }
                this.mrv32worker.postMessage({ event: 'performOneCycle', payload: null });
            }
        });
    }
}

export function instantiateEmulator() {
    return new Emulator();
}