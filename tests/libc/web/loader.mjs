// Xenolith wasm host runtime for the Roma import ABI (imported memory + sprt.*).
export function createRuntime({ onStdout, onStderr, onExit, argv0 = "libctest" } = {}) {
  const memory = new WebAssembly.Memory({ initial: 512, maximum: 16384 }); // 32MiB→1GiB
  const dec = new TextDecoder();
  const enc = new TextEncoder();
  const u8 = () => new Uint8Array(memory.buffer);
  const dv = () => new DataView(memory.buffer);
  const readStr = (ptr, len) => dec.decode(u8().subarray(ptr, ptr + len));
  const timeOrigin = (typeof performance !== "undefined" && performance.timeOrigin) || Date.now();

  const args = [argv0];   // argv
  const env  = [];        // environ (empty)
  const packed = (list) => {
    const bytes = list.reduce((n, s) => n + enc.encode(s).length + 1, 0);
    return ((list.length & 0xffff) << 16) | (bytes & 0xffff);
  };
  const copy = (list) => (tablePtr, bufPtr) => {
    let p = bufPtr;
    for (let i = 0; i < list.length; i++) {
      dv().setUint32(tablePtr + i * 4, p, true);
      const b = enc.encode(list[i]);
      u8().set(b, p); p += b.length;
      u8()[p++] = 0;
    }
    return 0;
  };

  const imports = {
    env: { memory },
    sprt: {
      clock_now(clkid) {
        if (clkid === 1) { const n = (typeof performance!=="undefined")?performance.now():Date.now(); return (n+timeOrigin)*1e6; }
        return Date.now() * 1e6;
      },
      fd_write(h, buf, len /*, off */) { const s = readStr(buf, len); (h===2?(onStderr||onStdout):onStdout)?.(s); return len; },
      fd_read(/* h, buf, len, off */) { return 0; }, // stdin EOF
      args_sizes() { return packed(args); },
      args_copy(t, b) { return copy(args)(t, b); },
      environ_sizes() { return packed(env); },
      environ_copy(t, b) { return copy(env)(t, b); },
      proc_exit(code) { onExit?.(code); throw { __exit: code }; },
    },
  };
  return { imports };
}

export async function run(bytes, io) {
  const rt = createRuntime(io);
  const { instance } = await WebAssembly.instantiate(bytes, rt.imports);
  try { instance.exports._start(); }
  catch (e) { if (e && typeof e === "object" && "__exit" in e) return e.__exit; throw e; }
  return 0;
}
