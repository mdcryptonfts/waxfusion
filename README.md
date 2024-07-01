## WaxFusion Smart Contracts

---

This repo contains a unit testing environment for WaxFusion's smart contracts. Since the contracts rely on certain core WAX Blockchain smart contracts (eosio.system, eosio.voters etc), "mock" versions of these contracts are also in this repo.

The mock system contracts contain enough logic to mimic things like voting rewards, renting CPU etc.

Unit testing was performed with [FUCK YEA Smart Contract Framework](https://github.com/nsjames/fuckyea?tab=readme-ov-file), developed by Nathan James from EOS.

Instructions can be found in the fuckyea repo linked above. Unit tests for each contract can be found in the /tests directory for each specific contract.

e.g. `cd pol.fusion && fuckyea test build`

The `dapp.fusion` and `pol.fusion` contracts are the ones that contain the unit tests, as they are the only contracts that act as "managers" and directly execute actions. `token.fusion` and `cpu.fusion` are just secondary contracts that get managed by the main 2 contracts, so their test cases are covered in the `dapp.fusion` and `pol.fusion` tests.

It's also important to note that a couple of things are commented out for production. To run the unit tests, certain functions/actions need to be uncommented.

Documentation for the WaxFusion contracts can be founds at [docs.waxfusion.io](https://docs.waxfusion.io).