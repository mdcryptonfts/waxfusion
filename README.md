## WaxFusion Smart Contracts

---

This repo contains a unit testing environment for WaxFusion's smart contracts. Since the contracts rely on certain core WAX Blockchain smart contracts (eosio.system, eosio.voters etc), "mock" versions of these contracts are also in this repo.

The mock system contracts contain enough logic to mimic things like voting rewards, renting CPU etc.

Unit testing was performed with [FUCK YEA Smart Contract Framework](https://github.com/nsjames/fuckyea?tab=readme-ov-file), developed by Nathan James from EOS.

Instructions can be found in the fuckyea repo linked above. Unit tests for each contract can be found in the /tests directory for each specific contract.

e.g. `cd pol.fusion && fuckyea test build`

It's important to note that at the time of this writing, only the pol.fusion contract has a full list of unit tests. The cpu.fusion and token.fusion contracts have been audited already, and the pol.fusion contract is currently undergoing audit.

The final dapp.fusion contract is still being refactored and prepared for audit.

Documentation for the WaxFusion contracts can be founds at [docs.waxfusion.io](https://docs.waxfusion.io). The dapp.fusion documentation still needs to be updated as the contract is refactored and prepared for auditing.