let inputBuffer = [];

var Module = {
    onRuntimeInitialized: () => {
        // expose Module functions
        self.addEventListener('message', (e) => {
            const data = e.data;
            // console.log('worker', data);

            if (data.event == 'initialize') {
                Module._Initialize();
                self.postMessage({ event: 'initializationEnd', payload: null });
            } else if (data.event == 'performOneCycle') {
                for (let i = 0; i < 8; i++) {
                    Module._PerformOneCycle();
                }

                const outputBufferPtr = Module._GetOutputBuffer();
                const outputBuffer = Module.HEAP8.slice(outputBufferPtr, outputBufferPtr + Module._GetOutputBufferLength());
                Module._ClearOutputBuffer();

                inputBuffer.forEach(ch => {
                    Module._SetInputBufferSymbol(ch);
                });
                inputBuffer = [];

                const decoder = new TextDecoder();
                const str = decoder.decode(outputBuffer);

                self.postMessage({ event: 'oneCycleEnd', payload: { data: str } });
            } else if (data.event == 'input') {
                inputBuffer.push(data.payload.data);
            }
        });

        self.postMessage({ event: 'ready', payload: null });
    }
};

importScripts('test.js');