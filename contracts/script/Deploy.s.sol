// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

import {Script, console2} from "forge-std/Script.sol";
import {FireflyPOAP} from "../src/FireflyPOAP.sol";

/**
 * @notice Deploy FireflyPOAP. Configure via env vars:
 *   PRIVATE_KEY               deployer key (becomes owner)
 *   POAP_ATTESTOR             Firefly device address (from poap_attestorAddress)
 *   POAP_EVENT_ID             uint event id (default 1)
 *   POAP_BASE_URI             metadata base, e.g. https://host/metadata/
 *   POAP_MAX_SUPPLY           0 = unlimited (default 0)
 *   POAP_ALLOW_MULTIPLE       true/false (default false)
 *
 * Example:
 *   forge script script/Deploy.s.sol --rpc-url base_sepolia --broadcast
 */
contract Deploy is Script {
    function run() external returns (FireflyPOAP poap) {
        uint256 pk = vm.envUint("PRIVATE_KEY");
        address attestor = vm.envAddress("POAP_ATTESTOR");
        uint256 eventId = vm.envOr("POAP_EVENT_ID", uint256(1));
        string memory baseURI =
            vm.envOr("POAP_BASE_URI", string("http://localhost:8787/metadata/"));
        uint256 maxSupply = vm.envOr("POAP_MAX_SUPPLY", uint256(0));
        bool allowMultiple = vm.envOr("POAP_ALLOW_MULTIPLE", false);

        vm.startBroadcast(pk);
        poap = new FireflyPOAP(
            "Firefly POAP",
            "FPOAP",
            attestor,
            eventId,
            baseURI,
            maxSupply,
            allowMultiple
        );
        vm.stopBroadcast();

        console2.log("FireflyPOAP deployed at:", address(poap));
        console2.log("attestor:", attestor);
        console2.log("eventId:", eventId);
    }
}
