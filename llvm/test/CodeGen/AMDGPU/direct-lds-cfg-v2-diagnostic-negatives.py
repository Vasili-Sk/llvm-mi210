# Diagnostic rejection matrix for the optimized ordinary HIP fixture.
import pathlib, subprocess, sys
source, stem, llc = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3]
base = source.read_text()

def once(name, text, reason):
    inp = pathlib.Path(str(stem) + "." + name + ".ll")
    inp.write_text(text)
    cpu = "gfx942" if name == "non-gfx90a" else ("gfx1100" if name == "wrong-wave" else "gfx90a")
    cmd = [llc, "-mtriple=amdgcn-amd-amdhsa", "-mcpu=" + cpu, "-O2",
           "-amdgpu-direct-lds-cfg-v2-diagnostic", "-filetype=null", str(inp)]
    run = subprocess.run(cmd, text=True, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, check=True)
    if '"status":"rejected"' not in run.stderr or reason not in run.stderr:
        raise SystemExit(f"{name}: expected {reason}: {run.stderr}")

missing_decl = base.replace("%1 = load <4 x i32>, ptr addrspace(1) %arrayidx, align 16, !tbaa !13", "%1 = call <4 x i32> @missing_producer()", 1) + "\ndeclare <4 x i32> @missing_producer()\n"
divergent = base.replace("  %arrayidx = getelementptr", "  %dbcond = icmp eq i32 %0, 0\n  %dbbase = select i1 %dbcond, ptr addrspace(1) %a.coerce, ptr addrspace(1) %b.coerce\n  %arrayidx = getelementptr", 1).replace("ptr addrspace(1) %a.coerce, i64 %idxprom", "ptr addrspace(1) %dbbase, i64 %idxprom", 1)
malformed = base.replace("  %idxprom = zext nneg i32 %0 to i64", "  %badcond = icmp eq i32 %0, 0\n  br i1 %badcond, label %bad.a, label %bad.b\nbad.a:\n  %badla = load volatile i32, ptr addrspace(1) %a.coerce, align 4\n  br label %bad.join\nbad.b:\n  %badlb = load volatile i32, ptr addrspace(1) %b.coerce, align 4\n  br label %bad.join\nbad.join:\n  %idxprom = zext nneg i32 %0 to i64", 1)
escape = base.replace("entry:\n", "entry:\n  store ptr addrspace(3) @_ZZ20cfg_v2_queue_extractE4tile, ptr addrspace(1) %out.coerce, align 8\n", 1)
shift_prelude = base.replace("[6144 x i32] undef", "[6145 x i32] undef", 1).replace("  %invariant.gep = getelementptr", "  %shift.and = add i32 %and.i, 1\n  %invariant.gep = getelementptr", 1)
producer_shift_only = shift_prelude.replace("ptr addrspace(3) %invariant.gep445, i32 %and.i", "ptr addrspace(3) %invariant.gep445, i32 %shift.and", 1)
consumer_shift_only = shift_prelude.replace("ptr addrspace(3) %invariant.gep463, i32 %and.i", "ptr addrspace(3) %invariant.gep463, i32 %shift.and", 1).replace("ptr addrspace(3) %63, i32 %and.i", "ptr addrspace(3) %63, i32 %shift.and", 1).replace("ptr addrspace(3) %76, i32 %and.i", "ptr addrspace(3) %76, i32 %shift.and", 1)

cases = {
  "producer-shift-only": (producer_shift_only, "region-base-mismatch"),
  "consumer-shift-only": (consumer_shift_only, "region-base-mismatch"),
  "no-fence": (base.replace('  fence syncscope("workgroup") release\n', '', 1), "workgroup-fence-order"),
  "missing-producer": (missing_decl, "producer-count"),
  "duplicate-region": (base.replace("%gep447.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 4096", "%gep447.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 0", 1), "layout-or-inverse-mismatch"),
  "escape": (escape, "alias-or-escape"),
  "atomic": (base.replace("%25 = load i32,", "%25 = load atomic i32,", 1).replace("%invariant.gep466, align 4", "%invariant.gep466 monotonic, align 4", 1), "unsupported-memory-flags"),
  "cache-mismatch": (base.replace("%1 = load <4 x i32>, ptr addrspace(1) %arrayidx, align 16, !tbaa !13", "%1 = load <4 x i32>, ptr addrspace(1) %arrayidx, align 16, !tbaa !13, !nontemporal !15", 1) + "\n!15 = !{i32 1}\n", "unsupported-memory-flags"),
  "divergent-base": (divergent, "divergent-base"),
  "early-overwrite": (base.replace('  tail call void @llvm.amdgcn.s.barrier()\n  fence syncscope("workgroup") acquire\n  %vecext197 = extractelement <4 x i32> %11, i64 0\n  store i32 %vecext197, ptr addrspace(3) %invariant.gep446, align 4, !tbaa !14', '  fence syncscope("workgroup") acquire\n  %vecext197 = extractelement <4 x i32> %11, i64 0\n  store i32 %vecext197, ptr addrspace(3) %invariant.gep446, align 4, !tbaa !14\n  tail call void @llvm.amdgcn.s.barrier()', 1), "early-overwrite"),
  "shared-region": (base.replace("%gep447.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 4096", "%gep447.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep446, i32 0", 1), "layout-or-inverse-mismatch"),
  "wrong-queue-order": (base.replace("%mul244 = mul i32 %stage_stride_x4, 3", "%mul244 = mul i32 %stage_stride_x4, 2", 1), "queue-order"),
  "malformed-cfg": (malformed, "control-flow-gap"),
  "missing-store": (base.replace("store i32 %vecext, ptr addrspace(3) %invariant.gep446", "store i32 %vecext, ptr addrspace(1) %out.coerce", 1), "store-count"),
  "missing-consumer": (base.replace("load i32, ptr addrspace(3) %invariant.gep466", "load i32, ptr addrspace(1) %a.coerce", 1), "consumer-count"),
  "wrong-inverse": (base.replace("%arrayidx138.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 32", "%arrayidx138.1 = getelementptr inbounds nuw i8, ptr addrspace(3) %invariant.gep466, i32 36", 1), "consumer-count"),
  "volatile": (base.replace("%1 = load <4 x i32>", "%1 = load volatile <4 x i32>", 1), "unsupported-memory-flags"),
  "incomplete-barrier": (base.replace("  tail call void @llvm.amdgcn.s.barrier()\n", "", 1), "barrier-count"),
  "wrong-workgroup": (base.replace('"amdgpu-flat-work-group-size"="1,256"', '"amdgpu-flat-work-group-size"="1,128"'), "workgroup-not-256"),
  "wrong-wave": (base.replace('"target-cpu"="gfx90a"', '"target-cpu"="gfx1100" "target-features"="+wavefrontsize32"'), "wave-not-64"),
  "non-gfx90a": (base.replace('"target-cpu"="gfx90a"', '"target-cpu"="gfx942"'), "target-not-gfx90a"),
  "extra-memory": (base.replace("entry:\n", "entry:\n  %extra = load volatile i32, ptr addrspace(1) %a.coerce, align 4\n", 1), "extra-memory-access"),
}
for name, (text, reason) in cases.items():
    once(name, text, reason)
