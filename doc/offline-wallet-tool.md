# xpchain-wallet: Standalone Offline Wallet Tool

`xpchain-wallet` is a standalone command-line tool that allows inspecting, creating, and recovering XPChain wallet files without running the node daemon (`xpchaind`) or connecting to the P2P network.

## Features

- **Zero Network Exposure**: Operates completely offline, making it ideal for air-gapped cold storage environments and exchange server maintenance.
- **Instant Operations**: Opens and executes commands directly against wallet database files within milliseconds without loading the blockchain.
- **Database Support**: Full support for modern SQLite/SQLCipher wallets as well as legacy Berkeley DB (BDB) wallets.

## Usage

```bash
xpchain-wallet [options] <command> [wallet_name]
```

### Commands

1. **`info`**: Display wallet metadata, database format, encryption status, and keypool metrics.
   ```bash
   xpchain-wallet -datadir=/path/to/data -wallet=my_wallet info
   ```
   **Example Output**:
   ```
   Wallet info
   ===========
   Name: my_wallet
   Format: sqlite
   Encrypted: no
   Descriptors: no
   HD (hierarchical deterministic): yes
   Keypool Size: 2000
   Transactions: 0
   Address Book Entries: 0
   ```

2. **`create`**: Create a new offline wallet file.
   ```bash
   # Create a modern SQLite wallet (default)
   xpchain-wallet -datadir=/path/to/data -wallet=my_new_wallet create

   # Create a legacy Berkeley DB wallet
   xpchain-wallet -datadir=/path/to/data -wallet=my_legacy_wallet -format=bdb create

   # Create a blank wallet without initial keys
   xpchain-wallet -datadir=/path/to/data -wallet=blank_wallet -blank create
   ```

3. **`salvage`**: Attempt to recover data from a corrupted Berkeley DB wallet file.
   ```bash
   xpchain-wallet -datadir=/path/to/data -wallet=corrupted_bdb salvage
   ```
