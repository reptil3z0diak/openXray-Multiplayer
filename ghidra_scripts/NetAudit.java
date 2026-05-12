// Ghidra headless post-script for xrMPE/OpenXRay network-oriented binary triage.
// Usage: -postScript NetAudit.java <output-directory>

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.io.File;
import java.io.PrintWriter;
import java.util.Locale;

public class NetAudit extends GhidraScript {
    private static final String[] KEYWORDS = {
        "gamenetworking", "steam", "socket", "send", "recv", "packet", "protobuf",
        "connect", "disconnect", "client", "server", "directplay", "dpn", "net_",
        "lua", "luajit", "luabind", "script", "spawn", "destroy", "inventory",
        "actor", "player", "entity", "replic", "sync", "event", "xrnet"
    };

    private String clean(String s) {
        if (s == null) {
            return "";
        }
        return s.replace('\t', ' ').replace('\r', ' ').replace('\n', ' ');
    }

    private boolean interesting(String s) {
        String v = clean(s).toLowerCase(Locale.ROOT);
        for (String k : KEYWORDS) {
            if (v.contains(k)) {
                return true;
            }
        }
        return false;
    }

    private String programTag() {
        return currentProgram.getName().replaceAll("[^A-Za-z0-9_.-]", "_");
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        File outDir = new File(args.length > 0 ? args[0] : ".");
        outDir.mkdirs();
        String tag = programTag();

        try (PrintWriter out = new PrintWriter(new File(outDir, tag + "_summary.tsv"), "UTF-8")) {
            out.println("field\tvalue");
            out.println("program\t" + clean(currentProgram.getExecutablePath()));
            out.println("image_base\t" + currentProgram.getImageBase());
            out.println("language\t" + clean(currentProgram.getLanguageID().toString()));
            out.println("compiler\t" + clean(currentProgram.getCompilerSpec().getCompilerSpecID().toString()));
            for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
                out.println("block\t" + clean(block.getName()) + " " + block.getStart() + "-" + block.getEnd() +
                    " r=" + block.isRead() + " w=" + block.isWrite() + " x=" + block.isExecute());
            }
        }

        try (PrintWriter out = new PrintWriter(new File(outDir, tag + "_imports.tsv"), "UTF-8")) {
            out.println("address\tname\tnamespace\treference_count\treferences");
            SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
            while (it.hasNext() && !monitor.isCancelled()) {
                Symbol s = it.next();
                if (!s.isExternal()) {
                    continue;
                }
                ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(s.getAddress());
                int count = 0;
                StringBuilder refText = new StringBuilder();
                while (refs.hasNext()) {
                    Reference r = refs.next();
                    if (count < 40) {
                        if (refText.length() > 0) {
                            refText.append(",");
                        }
                        refText.append(r.getFromAddress());
                    }
                    count++;
                }
                out.println(s.getAddress() + "\t" + clean(s.getName(true)) + "\t" +
                    clean(s.getParentNamespace().getName(true)) + "\t" + count + "\t" + refText);
            }
        }

        try (PrintWriter out = new PrintWriter(new File(outDir, tag + "_interesting_strings.tsv"), "UTF-8")) {
            out.println("address\tlength\tvalue");
            DataIterator dataIt = currentProgram.getListing().getDefinedData(true);
            while (dataIt.hasNext() && !monitor.isCancelled()) {
                Data data = dataIt.next();
                Object value = data.getValue();
                if (value == null) {
                    continue;
                }
                String text = value.toString();
                if (text.length() >= 4 && interesting(text)) {
                    out.println(data.getAddress() + "\t" + text.length() + "\t" + clean(text));
                }
            }
        }

        try (PrintWriter out = new PrintWriter(new File(outDir, tag + "_interesting_functions.tsv"), "UTF-8")) {
            out.println("entry\tname\tsize\tcalled_interesting_symbols\tstring_refs\tcall_refs");
            FunctionIterator funcs = currentProgram.getFunctionManager().getFunctions(true);
            while (funcs.hasNext() && !monitor.isCancelled()) {
                Function fn = funcs.next();
                StringBuilder calls = new StringBuilder();
                StringBuilder strings = new StringBuilder();
                int callCount = 0;
                int strCount = 0;

                InstructionIterator ins = currentProgram.getListing().getInstructions(fn.getBody(), true);
                while (ins.hasNext() && !monitor.isCancelled()) {
                    Instruction inst = ins.next();
                    for (Reference ref : inst.getReferencesFrom()) {
                        Address to = ref.getToAddress();
                        Symbol sym = currentProgram.getSymbolTable().getPrimarySymbol(to);
                        if (sym != null && interesting(sym.getName(true))) {
                            if (callCount < 30) {
                                if (calls.length() > 0) {
                                    calls.append(" | ");
                                }
                                calls.append(inst.getAddress()).append("->").append(clean(sym.getName(true)));
                            }
                            callCount++;
                        }
                        Data data = currentProgram.getListing().getDefinedDataAt(to);
                        if (data != null && data.getValue() != null && interesting(data.getValue().toString())) {
                            if (strCount < 20) {
                                if (strings.length() > 0) {
                                    strings.append(" | ");
                                }
                                strings.append(to).append("=").append(clean(data.getValue().toString()));
                            }
                            strCount++;
                        }
                    }
                }

                boolean nameHit = interesting(fn.getName(true));
                if (nameHit || callCount > 0 || strCount > 0) {
                    out.println(fn.getEntryPoint() + "\t" + clean(fn.getName(true)) + "\t" +
                        fn.getBody().getNumAddresses() + "\t" + clean(calls.toString()) + "\t" +
                        clean(strings.toString()) + "\t" + callCount);
                }
            }
        }
    }
}
