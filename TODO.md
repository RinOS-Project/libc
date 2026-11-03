# libc TODO
未完了項目はリポジトリrootの [`TODO.md`](../../../TODO.md) を正本とする。このファイルには完了記録のみを残す。
- [x] **LIBC-ENV-NOOP-RECLAIM-SCAN-01 / P2 / M** — 既存変数への `setenv(..., 0)` と未登録名への `unsetenv()` は環境を変更せず、reader epoch同期やallocation全走査を行わない。変更失敗後の回収と、並行reader中の環境更新の安全性を保つ。


- [x] **LIBC-PTHREAD-STACKADDR-LEGACY-API-01 / P3 / S** — 下向き成長するx86 stackのlegacy high addressを`pthread_attr_setstack()`のlow baseへ変換し、getterは逆変換する。stack size、null、undersized、underflow／overflow検証とtarget capabilityを維持する。


- [x] **LIBC-FERAISEEXCEPT-TRAP-DELIVERY-01 / P2 / M** — `feraiseexcept()`はx87 statusとMXCSRのsticky bitを直接書き換えるため、対応例外のtrapがunmaskedでも`#MF`／`#XM`を発生させず、フラグ設定だけで成功を返す。WG14の規定はenabled trapも取る意図を示す。**受入:** 対象例外を実際に発生させるx87／SSE操作を安全に選び、overflow／underflow時の追加`FE_INEXACT`や複数例外の順序を含むC実装契約を定義する。x86／x86_64でunmasked trapが`SIGFPE`として配送され、masked時のflag union、既存フラグ、rounding mode、`feraiseexcept()`の戻り値を保つことをruntime確認する。

- [x] **LIBC-POSIX-REGEX-LOCALE-SEMANTICS-01 / P2 / L** — `regex.h`の`regcomp()`／`regexec()`はbyte単位のbounded subsetで、`[:class:]`は`_regex_ascii_class()`がASCII byte範囲から作り、collating element／equivalence classは1 byte以外を`REG_ECOLLATE`で拒否する。POSIX regexのlocale依存文字クラスと多byte collating sequenceを扱えず、`sysconf(_SC_REGEXP)`も未対応を返す。localeの文字分類・照合順序をregex compile／matchへ通し、UTF-8など多byte文字とcollating/equivalence要素を処理する。**受入:** C/POSIX localeに加え多byte localeで、文字クラス・範囲・collating/equivalence要素の`regcomp()`／`regexec()`結果を確認し、locale変更時のcompiled regexの意味を定義する。POSIX契約を満たすまでは`_SC_REGEXP`を未対応のままにする。
- [x] **LIBC-WIDE-SCANF-ISWSPACE-01 / P2 / M** — `fwscanf()`／`swscanf()`のformat空白、先行空白skip、`%s`境界はwide ctype localeに従う。C/POSIXのASCII動作と、wide string／stream双方のU+3000処理を保つ。
