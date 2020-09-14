SimpleBus
---------

- Simple bus model
- No limitation on number of pending transactions (all targets that can return
  false must support multiple transactions)
- added support for DMI and debug transactions
- AT mode:
-- Incoming transactions are queued
-- AT protocol is executed from a different SC_THREAD
-- A target is notified immediately of the end of a transaction (using timing
   annotation). This is needed because the initiator can re-use the
   transaction (and the target may use the transaction pointer to identify the
   transaction)
