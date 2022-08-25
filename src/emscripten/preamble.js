Module['engine_ready'] = false
Module['send_command'] = function () { }
Module['read_stdout'] = function (output) { console.log(output) }
Module['preRun'] = [
    function () {
        let input = {
            str: '',
            index: 0,
            set: function (str) {
                this.str = str + '\n'
                this.index = 0
            },
        }

        let output = {
            str: '',
            flush: function () {
                Module.read_stdout(this.str)
                this.str = ''
            },
        }

        function stdin() {
            // Return ASCII code of character, or null if no input
            let char = input.str.charCodeAt(input.index++)
            return isNaN(char) ? null : char
        }

        function stdout(char) {
            if (!char || char == '\n'.charCodeAt(0)) {
                output.flush()
            } else {
                output.str += String.fromCharCode(char)
            }
        }

        FS.init(stdin, stdout, stdout)
        let wasm_uci_execute = Module.cwrap('wasm_uci_execute', 'void', [])
        Module.send_command = function (data) {
            input.set(data)
            wasm_uci_execute()
        }
    }
]
Module['onRuntimeInitialized'] = function () {
    Module.engine_ready = true
}