#include "EmscriptenFileDialog.h"

#include <emscripten.h>
#include <iostream>
#include <cstring>

void emcriptenFileDialog(int handler) {

    EM_ASM({

        function openFileDialog() {

            var handler = $0;

            var input = document.createElement('input');
            input.type = 'file';
            input.style.display = 'none';

            input.addEventListener('change', function () {
                if (input.files.length > 0) {
                    var file = input.files[0];
                    var reader = new FileReader();
                    
                    reader.onload = function(e) {
                        var fileContent = new Uint8Array(e.target.result);
                        var fileName = file.name;
                        var fileSize = fileContent.length;

                        // Pass file data and name to C++
                        var buffer = Module._malloc(fileSize);
                        Module.HEAPU8.set(fileContent, buffer);

                        Module.ccall('processFile', null, ['number', 'string', 'number', 'number'], [handler, fileName, fileSize, buffer]);

                        // Free allocated memory
                        Module._free(buffer);
                    };
                    
                    reader.readAsArrayBuffer(file); // Lê o arquivo como ArrayBuffer para binários
                }
            });

            document.body.appendChild(input);
            input.click();
            document.body.removeChild(input);
        }
        openFileDialog();
        
    }, handler);
}

