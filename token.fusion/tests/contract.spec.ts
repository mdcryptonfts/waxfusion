const { Blockchain, nameToBigInt } = require("@eosnetwork/vert");
const { assert } = require("chai");
const blockchain = new Blockchain()

// Load contract (use paths relative to the root of the project)
const contract = blockchain.createContract('token', 'build/token')

/* Runs before each test */
beforeEach(async () => {
    blockchain.resetTables()
})

/* Tests */
describe('Test', () => {
    it('testing', async () => {
        const [usera, userb] = blockchain.createAccounts('usera', 'userb')
        const result = await contract.actions.create(['usera', '100.00000000 WAX']).send('usera@active')
        console.log('success')
    });
});
