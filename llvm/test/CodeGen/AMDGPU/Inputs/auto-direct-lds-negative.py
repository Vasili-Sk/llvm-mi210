#!/usr/bin/env python3
"""Generate named near-matches and prove automatic direct-LDS rejection."""
import pathlib
import re
import subprocess
import sys
import tempfile

opt, template = sys.argv[1:3]
llc = str(pathlib.Path(opt).with_name("llc"))
text = pathlib.Path(template).read_text()
# Test input ends after attributes. Keep only IR, without RUN and CHECK lines.
base = "\n".join(x for x in text.splitlines() if not x.startswith("; RUN:") and not x.startswith("; CHECK")) + "\n"


def rename(s, name):
    return s.replace("@auto_fuse_x4", "@" + name)


def replace_once(s, old, new):
    if s.count(old) != 1:
        raise AssertionError((old, s.count(old)))
    return s.replace(old, new, 1)


def add_before_end(s, ir):
    marker = "\n}\n\n; Function Attrs: convergent mustprogress nocallback"
    return replace_once(s, marker, ir + "\n}\n\n; Function Attrs: convergent mustprogress nocallback")


def remove_line(s, needle):
    lines = s.splitlines()
    matches = [i for i, line in enumerate(lines) if needle in line]
    if len(matches) != 1:
        raise AssertionError((needle, matches))
    del lines[matches[0]]
    return "\n".join(lines) + "\n"


def width_variant(s, width):
    old = "%7 = load <4 x i32>, ptr addrspace(1) %gep, align 16"
    s = replace_once(s, old, f"%7 = load <{width} x i32>, ptr addrspace(1) %gep, align 16")
    for i in range(4):
        s = s.replace(f"extractelement <4 x i32> %7, i64 {i}",
                      f"extractelement <{width} x i32> %7, i64 {i % width}")
    return s


def split_conditional_store(s):
    old = "  store i32 %8, ptr addrspace(3) %arrayidx10, align 4\n  %9 ="
    new = "  %store.cond = icmp eq i32 %and, 0\n  br i1 %store.cond, label %store.then, label %store.cont\n\nstore.then:\n  store i32 %8, ptr addrspace(3) %arrayidx10, align 4\n  br label %store.cont\n\nstore.cont:\n  %9 ="
    s = replace_once(s, old, new)
    s = s.replace("[ %xor39, %for.body ]", "[ %xor39, %store.cont ]")
    s = s.replace("[ %inc, %for.body ]", "[ %inc, %store.cont ]")
    return s


def branch_loop(s):
    old = "  %exitcond.not = icmp eq i32 %inc, 8\n  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body"
    new = "  %exitcond.not = icmp eq i32 %inc, 8\n  %branch.cond = icmp eq i32 %and, 0\n  br i1 %branch.cond, label %loop.branch, label %loop.branch2\n\nloop.branch:\n  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body\n\nloop.branch2:\n  call void @llvm.sideeffect()\n  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body"
    s = replace_once(s, old, new)
    s = s.replace("[ %xor39, %for.body ]", "[ %xor39, %loop.branch ], [ %xor39, %loop.branch2 ]")
    s = s.replace("[ %inc, %for.body ]", "[ %inc, %loop.branch ], [ %inc, %loop.branch2 ]")
    s += "declare void @llvm.sideeffect()\n"
    return s


def two_pred_merge(s):
    old = "  br label %for.body\n\nfor.cond.cleanup:"
    new = "  %merge.cond = icmp eq i32 %and, 0\n  br i1 %merge.cond, label %left, label %right\n\nleft:\n  br label %for.body\n\nright:\n  call void @llvm.sideeffect()\n  br label %for.body\n\nfor.cond.cleanup:"
    s = replace_once(s, old, new)
    s = s.replace("%Acc.090 = phi i32 [ %xor, %entry ]", "%Acc.090 = phi i32 [ %xor, %left ], [ %xor, %right ]")
    s = s.replace("%Trip.089 = phi i32 [ 0, %entry ]", "%Trip.089 = phi i32 [ 0, %left ], [ 0, %right ]")
    s += "declare void @llvm.sideeffect()\n"
    return s


def nested_loop(s):
    old = "  br label %for.body\n\nfor.cond.cleanup:"
    new = "  br label %inner\n\ninner:\n  %inner.i = phi i32 [ 0, %entry ], [ %inner.next, %inner ]\n  %inner.next = add i32 %inner.i, 1\n  %inner.done = icmp eq i32 %inner.next, 2\n  br i1 %inner.done, label %for.body, label %inner\n\nfor.cond.cleanup:"
    s = replace_once(s, old, new)
    s = s.replace("%Acc.090 = phi i32 [ %xor, %entry ]", "%Acc.090 = phi i32 [ %xor, %inner ]")
    s = s.replace("%Trip.089 = phi i32 [ 0, %entry ]", "%Trip.089 = phi i32 [ 0, %inner ]")
    return s


def non_dominating(s):
    # A valid phi exposes that the producer is present on only one predecessor.
    load = "  %7 = load <4 x i32>, ptr addrspace(1) %gep, align 16"
    s = replace_once(s, load, "  %load.cond = icmp eq i32 %and, 0\n  br i1 %load.cond, label %load.then, label %load.other\n\nload.then:\n  %7 = load <4 x i32>, ptr addrspace(1) %gep, align 16\n  br label %load.merge\n\nload.other:\n  br label %load.merge\n\nload.merge:\n  %loaded = phi <4 x i32> [ %7, %load.then ], [ zeroinitializer, %load.other ]")
    s = s.replace("extractelement <4 x i32> %7", "extractelement <4 x i32> %loaded")
    s = s.replace("[ %xor39, %for.body ]", "[ %xor39, %load.merge ]")
    s = s.replace("[ %inc, %for.body ]", "[ %inc, %load.merge ]")
    return s


def unrelated(_):
    return '''target triple = "amdgcn-amd-amdhsa"
define amdgpu_kernel void @unrelated_classic(ptr addrspace(1) %p) #0 {
entry:
  %x = load i32, ptr addrspace(1) %p, align 4
  %y = add i32 %x, 1
  store i32 %y, ptr addrspace(1) %p, align 4
  ret void
}
attributes #0 = { "amdgpu-flat-work-group-size"="64,64" "target-cpu"="gfx90a" }
'''


def trip_predicate(s, pred, reverse=False):
    s = s.replace("icmp eq i32 %inc, 8", f"icmp {pred} i32 %inc, 8")
    if reverse:
        s = s.replace("br i1 %exitcond.not, label %for.cond.cleanup, label %for.body",
                      "br i1 %exitcond.not, label %for.body, label %for.cond.cleanup")
    return s


def zero_trip_bypass(s):
    s = replace_once(s, "  br label %for.body\n\nfor.cond.cleanup:",
                     "  %zero.bypass = icmp eq i32 %and, %and\n"
                     "  br i1 %zero.bypass, label %zero.exit, label %for.body\n\n"
                     "zero.exit:\n  ret void\n\nfor.cond.cleanup:")
    return s


def multiple_exits(s):
    s = replace_once(s, "  %exitcond.not = icmp eq i32 %inc, 8",
                     "  %early.exit = icmp eq i32 %and, 17\n"
                     "  br i1 %early.exit, label %second.exit, label %latch\n\n"
                     "second.exit:\n  ret void\n\nlatch:\n"
                     "  %exitcond.not = icmp eq i32 %inc, 8")
    s = s.replace("[ %xor39, %for.body ]", "[ %xor39, %latch ]")
    s = s.replace("[ %inc, %for.body ]", "[ %inc, %latch ]")
    return s


def offset_case(s, constant):
    old = "%gep = getelementptr inbounds nuw [16 x i8], ptr addrspace(1) %invariant.gep, i64 %mul6"
    new = (f"%offset.base = add i64 %conv7, {constant}\n  "
           "%gep = getelementptr inbounds nuw i8, ptr addrspace(1) %2, i64 %offset.base")
    return replace_once(s, old, new)


def large_invariant_gep(s, constant):
    old = "%2 = getelementptr inbounds nuw i8, ptr addrspace(1) %Input.coerce, i64 %.idx62"
    new = (f"%large.base = getelementptr i8, ptr addrspace(1) %Input.coerce, i64 {constant}\n  "
           "%2 = getelementptr inbounds nuw i8, ptr addrspace(1) %large.base, i64 %.idx62")
    return replace_once(s, old, new)

cases = []
def case(name, reason, fn): cases.append((name, reason, fn))

case("reject_non_gfx90a", "unsupported target", lambda s: s.replace('"target-cpu"="gfx90a"', '"target-cpu"="gfx942"'))
case("reject_wave32", "wave32", lambda s: s.replace('"target-cpu"="gfx90a"', '"target-cpu"="gfx90a" "target-features"="+wavefrontsize32"'))
case("reject_missing_occupancy_contract", "missing enforced occupancy contract", lambda s: s.replace(' "amdgpu-waves-per-eu"="8,8"', ""))
case("reject_nonexact_occupancy_contract", "inconsistent occupancy floor and cap", lambda s: s.replace('"amdgpu-waves-per-eu"="8,8"', '"amdgpu-waves-per-eu"="8,10"'))
case("reject_occupancy_9", "unsupported equal occupancy 9,9", lambda s: s.replace('"amdgpu-waves-per-eu"="8,8"', '"amdgpu-waves-per-eu"="9,9"'))
case("reject_occupancy_99", "unsupported equal occupancy 99,99", lambda s: s.replace('"amdgpu-waves-per-eu"="8,8"', '"amdgpu-waves-per-eu"="99,99"'))
case("reject_occupancy_zero", "zero occupancy contract", lambda s: s.replace('"amdgpu-waves-per-eu"="8,8"', '"amdgpu-waves-per-eu"="0,0"'))
case("reject_occupancy_below_max", "occupancy below architectural maximum", lambda s: s.replace('"amdgpu-waves-per-eu"="8,8"', '"amdgpu-waves-per-eu"="7,7"'))
case("reject_occupancy_malformed", "malformed occupancy contract", lambda s: s.replace('"amdgpu-waves-per-eu"="8,8"', '"amdgpu-waves-per-eu"="eight"'))
case("reject_excessive_static_lds", "static LDS prevents maximum occupancy", lambda s: s.replace('@tile = internal unnamed_addr addrspace(3) global [256 x i32] undef, align 1024', '@tile = internal unnamed_addr addrspace(3) global [65536 x i8] undef, align 1024'))
case("reject_incompatible_workgroup", "workgroup size is not exactly 64", lambda s: s.replace('"amdgpu-flat-work-group-size"="64,64"', '"amdgpu-flat-work-group-size"="128,128"'))
for width, label in ((1,"x1"),(2,"x2"),(3,"x3"),(5,"unsupported width")):
    case(f"reject_width_x{width}", label, lambda s, w=width: width_variant(s, w))
case("reject_dynamic_trip", "unknown trip count", lambda s: s.replace("%Output.coerce)", "%Output.coerce, i32 %Trips)").replace("icmp eq i32 %inc, 8", "icmp eq i32 %inc, %Trips"))
case("accept_predicate_ne", "proved ne predicate with true backedge", lambda s: trip_predicate(s, "ne", True))
case("reject_predicate_ult_true_exit", "ult predicate with true exit", lambda s: trip_predicate(s, "ult"))
case("reject_predicate_ule_true_exit", "ule predicate with true exit", lambda s: trip_predicate(s, "ule"))
case("reject_predicate_slt_true_exit", "slt predicate with true exit", lambda s: trip_predicate(s, "slt"))
case("reject_reversed_successors", "reversed latch successors", lambda s: trip_predicate(s, "eq", True))
case("reject_nonzero_start_one_trip", "nonzero induction start", lambda s: s.replace("%Trip.089 = phi i32 [ 0,", "%Trip.089 = phi i32 [ 7,"))
case("reject_step_other_than_one", "unsupported non-unit wrapping step", lambda s: s.replace("%inc = add nuw nsw i32 %Trip.089, 1", "%inc = add i32 %Trip.089, 2").replace("%Trip.089 = phi i32 [ 0,", "%Trip.089 = phi i32 [ 1,"))
case("reject_induction_wrap", "one-trip induction wrap", lambda s: s.replace("%Trip.089 = phi i32 [ 0,", "%Trip.089 = phi i32 [ -1,").replace("icmp eq i32 %inc, 8", "icmp eq i32 %inc, 0"))
case("reject_multiple_exits", "noncanonical CFG containing multiple loop exits", multiple_exits)
case("accept_offset_u32_boundary", "maximum aligned in-range offset", lambda s: offset_case(s, 4294967217))
case("reject_offset_u32_plus_one", "offset exceeds UINT32_MAX byte range", lambda s: offset_case(s, 4294967218))
case("reject_negative_offset", "negative global offset", lambda s: offset_case(s, -16))
case("reject_offset_arithmetic_wrap", "wrapping offset arithmetic", lambda s: offset_case(s, 18446744073709551600))
case("accept_large_invariant_gep", "sound folded invariant GEP", lambda s: large_invariant_gep(s, 1073741824))
case("reject_4g_invariant_gep", "4294967296-byte invariant GEP", lambda s: large_invariant_gep(s, 4294967296))
case("reject_zero_trip_bypass", "noncanonical CFG containing a zero-trip preheader bypass", zero_trip_bypass)
case("reject_one_stage", "one-stage unprofitable", lambda s: s.replace("icmp eq i32 %inc, 8", "icmp eq i32 %inc, 1"))
case("reject_bad_payload_permutation", "producer layout mismatch", lambda s: s.replace("extractelement <4 x i32> %7, i64 1", "extractelement <4 x i32> %7, i64 99").replace("extractelement <4 x i32> %7, i64 2", "extractelement <4 x i32> %7, i64 1").replace("extractelement <4 x i32> %7, i64 99", "extractelement <4 x i32> %7, i64 2"))
case("reject_shifted_lds_base", "incompatible LDS base", lambda s: replace_once(s, "%4 = getelementptr inbounds nuw i8, ptr addrspace(3) @tile, i32 %.idx85", "%shift = add i32 %.idx85, 4\n  %4 = getelementptr inbounds nuw i8, ptr addrspace(3) @tile, i32 %shift"))
case("reject_partial_producer", "partial producer stores", lambda s: remove_line(s, "store i32 %11,"))
case("reject_duplicate_producer_destination", "duplicate producer destination", lambda s: s.replace("store i32 %9, ptr addrspace(3) %arrayidx13", "store i32 %9, ptr addrspace(3) %arrayidx10"))
case("reject_missing_payload", "missing payload word", lambda s: s.replace("%11 = extractelement <4 x i32> %7, i64 3", "%11 = extractelement <4 x i32> %7, i64 2"))
case("reject_extra_producer_use", "extra producer use", lambda s: replace_once(s, "  %8 = extractelement", "  %extra = extractelement <4 x i32> %7, i64 0\n  %8 = extractelement").replace("  %add32 = add i32 %12, %Acc.090", "  %extra.sum = xor i32 %extra, %12\n  %add32 = add i32 %extra.sum, %Acc.090"))
case("reject_extra_lds_store", "extra LDS store", lambda s: s.replace("  fence syncscope(\"workgroup\") release", "  store i32 %8, ptr addrspace(3) %arrayidx10, align 4\n  fence syncscope(\"workgroup\") release", 1))
case("reject_tile_pointer_escape", "escaping LDS pointer", lambda s: replace_once(s, "  %8 = extractelement", "  %escaped = ptrtoint ptr addrspace(3) %arrayidx10 to i32\n  %8 = extractelement").replace("  %add32 = add i32 %12, %Acc.090", "  %escape.sum = xor i32 %escaped, %12\n  %add32 = add i32 %escape.sum, %Acc.090"))
case("reject_unknown_alias", "unknown aliasing memory operation", lambda s: replace_once(s, "  %8 = extractelement", "  %alias = load volatile i32, ptr addrspace(1) %gep, align 4\n  %8 = extractelement").replace("  %add32 = add i32 %12, %Acc.090", "  %alias.sum = xor i32 %alias, %12\n  %add32 = add i32 %alias.sum, %Acc.090"))
case("reject_extra_consumer", "extra consumer", lambda s: replace_once(s, "  %add32 =", "  %extra.read = load volatile i32, ptr addrspace(3) %arrayidx10, align 4\n  %extra.consumer.sum = xor i32 %extra.read, %12\n  %add32 =").replace("%add32 = add i32 %12", "%add32 = add i32 %extra.consumer.sum"))
case("reject_partial_consumer", "partial consumer", lambda s: remove_line(s, "%15 = load i32").replace("%mul38 = mul i32 %15", "%mul38 = mul i32 0"))
case("reject_duplicate_consumer", "duplicate consumer destination", lambda s: s.replace("%15 = load i32, ptr addrspace(3) %arrayidx19", "%15 = load i32, ptr addrspace(3) %arrayidx16"))
case("reject_unsupported_ds_read", "unsupported DS read width", lambda s: replace_once(s, "  %12 = load i32", "  %wide.read = load volatile <2 x i32>, ptr addrspace(3) %arrayidx10, align 4\n  %wide.word = extractelement <2 x i32> %wide.read, i64 0\n  %12 = load i32").replace("  %add32 = add i32 %12, %Acc.090", "  %wide.sum = xor i32 %wide.word, %12\n  %add32 = add i32 %wide.sum, %Acc.090"))
case("reject_volatile_mismatch", "volatile memory mismatch", lambda s: s.replace("%7 = load <4 x i32>", "%7 = load volatile <4 x i32>"))
case("reject_atomic_access", "atomic LDS access", lambda s: replace_once(s, "  %8 = extractelement", "  %atomic = atomicrmw add ptr addrspace(3) %arrayidx10, i32 1 seq_cst\n  %8 = extractelement"))
case("reject_nontemporal_cache", "non-temporal cache mismatch", lambda s: s.replace("%7 = load <4 x i32>, ptr addrspace(1) %gep, align 16", "%7 = load <4 x i32>, ptr addrspace(1) %gep, align 16, !nontemporal !0") + "!0 = !{i32 1}\n")
case("reject_non_dominating_producer", "noncanonical CFG containing a non-dominating producer", non_dominating)
case("reject_conditional_store", "noncanonical CFG containing a conditional store", split_conditional_store)
case("reject_branch_inside_loop", "noncanonical CFG containing a branch inside the loop", branch_loop)
case("reject_two_predecessor_merge", "noncanonical CFG containing a two-predecessor merge", two_pred_merge)
case("reject_call_in_loop", "call in loop", lambda s: replace_once(s, "  %8 = extractelement", "  call void @external()\n  %8 = extractelement") + "declare void @external()\n")
case("reject_changing_global_base", "changing global base", lambda s: s.replace("%Output.coerce)", "%Output.coerce, ptr addrspace(1) %Input2)").replace("  %Acc.090 = phi", "  %base.phi = phi ptr addrspace(1) [ %invariant.gep, %entry ], [ %Input2, %for.body ]\n  %Acc.090 = phi").replace("ptr addrspace(1) %invariant.gep, i64 %mul6", "ptr addrspace(1) %base.phi, i64 %mul6"))
case("reject_changing_lds_base", "changing LDS base", lambda s: replace_once(s, "  %4 = getelementptr", "  %lds.cond = icmp eq i32 %and, 0\n  %lds.select = select i1 %lds.cond, ptr addrspace(3) @tile, ptr addrspace(3) null\n  %4 = getelementptr").replace("ptr addrspace(3) @tile, i32 %.idx85", "ptr addrspace(3) %lds.select, i32 %.idx85"))
case("reject_multiple_waves", "multiple waves", lambda s: s.replace('"amdgpu-flat-work-group-size"="64,64"', '"amdgpu-flat-work-group-size"="128,128"'))
case("reject_wrong_workgroup", "wrong workgroup size", lambda s: s.replace('"amdgpu-flat-work-group-size"="64,64"', '"amdgpu-flat-work-group-size"="32,32"'))
case("reject_nested_loop", "noncanonical CFG containing a nested loop", nested_loop)
case("reject_unrelated_classic", "unrelated classic function", unrelated)

with tempfile.TemporaryDirectory() as td:
    td = pathlib.Path(td)
    for name, reason, mutate in cases:
        inp = td / f"{name}.ll"
        out = td / f"{name}.out.ll"
        candidate = rename(mutate(base), name)
        if name in {"reject_non_dominating_producer",
                    "reject_two_predecessor_merge"}:
            blocks = re.findall(r"(?m)^[A-Za-z$._][A-Za-z0-9$._-]*:\s*(?:;.*)?$",
                                candidate)
            if len(blocks) == 3:
                raise AssertionError(f"{name}: did not exceed the three-block canonical CFG")
        inp.write_text(candidate)
        baseline = td / f"{name}.baseline.ll"
        baseline_cmd = [opt, "-mtriple=amdgcn-amd-amdhsa", "-mcpu=gfx90a",
                        "-passes=verify", "-S", str(inp), "-o", str(baseline)]
        baseline_run = subprocess.run(baseline_cmd, text=True,
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if baseline_run.returncode:
            raise AssertionError(f"{name}: baseline normalization failed\n{baseline_run.stderr}")
        cmd = [opt, "-mtriple=amdgcn-amd-amdhsa", "-mcpu=gfx90a",
               "-passes=amdgpu-auto-direct-lds", "-verify-each", "-S",
               str(inp), "-o", str(out)]
        p = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if p.returncode:
            raise AssertionError(f"{name}: opt failed\n{p.stderr}\n{candidate}")
        result = out.read_text()
        accepted = name.startswith("accept_")
        if accepted:
            if result.count("call void @llvm.amdgcn.global.load.lds.base") != 1:
                raise AssertionError(f"{name}: proved case did not transform exactly once")
            print(f"MATRIX PASS {name}: {reason}; proved and transformed")
            continue
        if result != baseline.read_text():
            raise AssertionError(f"{name}: rejected IR changed")
        if "call void @llvm.amdgcn.global.load.lds.base" in result:
            raise AssertionError(f"{name}: transformed unexpectedly")
        if name == "reject_occupancy_malformed":
            asm = td / f"{name}.s"
            codegen = subprocess.run([llc, "-O2", "-verify-machineinstrs",
                                      str(out), "-o", str(asm)], text=True,
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE)
            if codegen.returncode == 0 or "can't parse first integer attribute" not in codegen.stderr:
                raise AssertionError(f"{name}: normal codegen did not reject malformed target input")
            if asm.exists() and asm.read_text():
                raise AssertionError(f"{name}: failed codegen produced assembly")
            print(f"MATRIX PASS {name}: {reason}; normalized IR unchanged; "
                  "normal codegen rejected with no assembly or direct opcode")
            continue
        if name != "reject_unrelated_classic":
            vector_load = r"load (?:volatile )?<[^>]+ x i32>"
            lds_store = r"store (?:volatile )?i32 .*ptr addrspace\(3\)"
            if len(re.findall(vector_load, result)) != len(re.findall(vector_load, candidate)):
                raise AssertionError(f"{name}: staged producer changed on rejection")
            if len(re.findall(lds_store, result)) != len(re.findall(lds_store, candidate)):
                raise AssertionError(f"{name}: staged stores changed on rejection")
        asm = td / f"{name}.s"
        codegen = subprocess.run([llc, "-O2", "-verify-machineinstrs", str(out),
                                  "-o", str(asm)], text=True,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if codegen.returncode:
            raise AssertionError(f"{name}: llc failed\n{codegen.stderr}")
        assembly = asm.read_text()
        if re.search(r"global_load_[^\n]*\blds\b", assembly):
            raise AssertionError(f"{name}: direct opcode emitted unexpectedly")
        suffix = "; staged IR and opcode remain classic"
        if name in {"reject_non_dominating_producer",
                    "reject_two_predecessor_merge"}:
            suffix += "; rejected by three-block structural gate"
        print(f"MATRIX PASS {name}: {reason}{suffix}")

    # Positive and idempotence checks use the same standalone transform.
    pos = td / "positive.ll"
    out = td / "positive.out.ll"
    pos.write_text(rename(base, "positive_x4"))
    subprocess.run([opt, "-mtriple=amdgcn-amd-amdhsa", "-mcpu=gfx90a",
                    "-passes=amdgpu-auto-direct-lds,amdgpu-auto-direct-lds",
                    "-verify-each", "-S", str(pos), "-o", str(out)], check=True)
    result = out.read_text()
    assert result.count("call void @llvm.amdgcn.global.load.lds.base") == 1
    assert "load <4 x i32>" not in result
    print("MATRIX PASS positive_idempotent: exact transform count=1 after two runs")
