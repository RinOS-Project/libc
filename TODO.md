# libc TODO

- [ ] **LIBC-PTHREAD-PRIO-PROTECT-01 / P2 / M** — `PTHREAD_PRIO_PROTECT`は定義済みだが、`pthread_mutexattr_setprotocol()`は常に`ENOTSUP`を返し、mutex属性／mutex本体のpriority-ceiling取得・設定APIもない。**受入:** ceiling値の設定・範囲検証と、mutex取得時の優先度昇格および解放時の復元をスケジューラの所有権・待機処理へ統合し、priority inheritance、timeout、cancel、異常終了との相互作用を確認する。
