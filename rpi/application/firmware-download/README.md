# application/firmware-download

`DownloadManager` deține dimensiunea declarată, octeții acceptați, contorul de
blocuri, retransmiterea identică și finalizarea fișierului temporar.
`UpdateManager` din `application/update` decide tranzițiile și curățarea la
întrerupere. `VerificationManager` din `application/verification` deține workerul
SHA-256.

`TODO(OTA)`: mutați contorul de blocuri UDS la limita Dcm înainte de a conecta
un flux OTA la același `DownloadManager`. Detaliile sunt în
[`update-pipeline.md`](../../../docs/architecture/update-pipeline.md).
