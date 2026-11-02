# libc TODO

- [ ] **LIBC-PTHREAD-PRIO-PROTECT-01 / P2 / M** — `PTHREAD_PRIO_PROTECT`は定義済みだが、`pthread_mutexattr_setprotocol()`は常に`ENOTSUP`を返し、mutex属性／mutex本体のpriority-ceiling取得・設定APIもない。**受入:** ceiling値の設定・範囲検証と、mutex取得時の優先度昇格および解放時の復元をスケジューラの所有権・待機処理へ統合し、priority inheritance、timeout、cancel、異常終了との相互作用を確認する。

- [ ] **LIBC-FERAISEEXCEPT-TRAP-DELIVERY-01 / P2 / M** — `feraiseexcept()`はx87 statusとMXCSRのsticky bitを直接書き換えるため、対応例外のtrapがunmaskedでも`#MF`／`#XM`を発生させず、フラグ設定だけで成功を返す。WG14の規定はenabled trapも取る意図を示す。**受入:** 対象例外を実際に発生させるx87／SSE操作を安全に選び、overflow／underflow時の追加`FE_INEXACT`や複数例外の順序を含むC実装契約を定義する。x86／x86_64でunmasked trapが`SIGFPE`として配送され、masked時のflag union、既存フラグ、rounding mode、`feraiseexcept()`の戻り値を保つことをruntime確認する。

- [ ] **LIBC-POSIX-REGEX-LOCALE-SEMANTICS-01 / P2 / L** — `regex.h`の`regcomp()`／`regexec()`はbyte単位のbounded subsetで、`[:class:]`は`_regex_ascii_class()`がASCII byte範囲から作り、collating element／equivalence classは1 byte以外を`REG_ECOLLATE`で拒否する。POSIX regexのlocale依存文字クラスと多byte collating sequenceを扱えず、`sysconf(_SC_REGEXP)`も未対応を返す。localeの文字分類・照合順序をregex compile／matchへ通し、UTF-8など多byte文字とcollating/equivalence要素を処理する。**受入:** C/POSIX localeに加え多byte localeで、文字クラス・範囲・collating/equivalence要素の`regcomp()`／`regexec()`結果を確認し、locale変更時のcompiled regexの意味を定義する。POSIX契約を満たすまでは`_SC_REGEXP`を未対応のままにする。
