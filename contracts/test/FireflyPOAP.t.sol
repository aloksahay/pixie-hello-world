// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

import {Test} from "forge-std/Test.sol";
import {FireflyPOAP} from "../src/FireflyPOAP.sol";

contract FireflyPOAPTest is Test {
    FireflyPOAP poap;

    // Deterministic "device" key standing in for the Firefly's secp256k1 key.
    uint256 attestorPk = 0xA11CE;
    address attestor;

    uint256 constant EVENT_ID = 42;

    address alice = address(0xA1);
    address bob = address(0xB0B);

    function setUp() public {
        attestor = vm.addr(attestorPk);
        poap = new FireflyPOAP(
            "Firefly POAP",
            "FPOAP",
            attestor,
            EVENT_ID,
            "https://poap.example/metadata/",
            0,
            false
        );
    }

    // Sign a claim exactly as the Firefly device must: EIP-712 digest, r‖s‖v with
    // v in {27,28}.
    function _sign(uint256 pk, uint256 eventId, bytes32 nonce)
        internal
        view
        returns (bytes memory)
    {
        bytes32 digest = poap.claimDigest(eventId, nonce);
        (uint8 v, bytes32 r, bytes32 s) = vm.sign(pk, digest);
        return abi.encodePacked(r, s, v);
    }

    function testValidMint() public {
        bytes32 nonce = keccak256("nonce-1");
        bytes memory sig = _sign(attestorPk, EVENT_ID, nonce);

        vm.prank(alice);
        uint256 tokenId = poap.mint(EVENT_ID, nonce, sig);

        assertEq(poap.ownerOf(tokenId), alice);
        assertEq(poap.balanceOf(alice), 1);
        assertTrue(poap.usedNonce(nonce));
    }

    function testReplayReverts() public {
        bytes32 nonce = keccak256("nonce-replay");
        bytes memory sig = _sign(attestorPk, EVENT_ID, nonce);

        vm.prank(alice);
        poap.mint(EVENT_ID, nonce, sig);

        vm.prank(bob);
        vm.expectRevert(FireflyPOAP.NonceUsed.selector);
        poap.mint(EVENT_ID, nonce, sig);
    }

    function testWrongSignerReverts() public {
        uint256 impostorPk = 0xBAD;
        bytes32 nonce = keccak256("nonce-impostor");
        bytes memory sig = _sign(impostorPk, EVENT_ID, nonce);

        vm.prank(alice);
        vm.expectRevert(FireflyPOAP.BadAttestor.selector);
        poap.mint(EVENT_ID, nonce, sig);
    }

    function testWrongEventReverts() public {
        bytes32 nonce = keccak256("nonce-event");
        // Signature is over a different event id than the contract enforces.
        bytes memory sig = _sign(attestorPk, EVENT_ID + 1, nonce);

        vm.prank(alice);
        vm.expectRevert(FireflyPOAP.WrongEvent.selector);
        poap.mint(EVENT_ID + 1, nonce, sig);
    }

    function testDoubleMintPerWalletReverts() public {
        bytes32 nonce1 = keccak256("n1");
        bytes32 nonce2 = keccak256("n2");
        bytes memory sig1 = _sign(attestorPk, EVENT_ID, nonce1);
        bytes memory sig2 = _sign(attestorPk, EVENT_ID, nonce2);

        vm.prank(alice);
        poap.mint(EVENT_ID, nonce1, sig1);

        vm.prank(alice);
        vm.expectRevert(FireflyPOAP.AlreadyHolder.selector);
        poap.mint(EVENT_ID, nonce2, sig2);
    }

    function testAllowMultiplePerWallet() public {
        FireflyPOAP multi = new FireflyPOAP(
            "Firefly POAP", "FPOAP", attestor, EVENT_ID, "uri://", 0, true
        );
        bytes32 n1 = keccak256("m1");
        bytes32 n2 = keccak256("m2");

        vm.startPrank(alice);
        bytes32 d1 = multi.claimDigest(EVENT_ID, n1);
        (uint8 v1, bytes32 r1, bytes32 s1) = vm.sign(attestorPk, d1);
        multi.mint(EVENT_ID, n1, abi.encodePacked(r1, s1, v1));
        bytes32 d2 = multi.claimDigest(EVENT_ID, n2);
        (uint8 v2, bytes32 r2, bytes32 s2) = vm.sign(attestorPk, d2);
        multi.mint(EVENT_ID, n2, abi.encodePacked(r2, s2, v2));
        vm.stopPrank();

        assertEq(multi.balanceOf(alice), 2);
    }

    function testMaxSupplyReverts() public {
        FireflyPOAP capped = new FireflyPOAP(
            "Firefly POAP", "FPOAP", attestor, EVENT_ID, "uri://", 1, false
        );
        bytes32 n1 = keccak256("c1");
        bytes32 d1 = capped.claimDigest(EVENT_ID, n1);
        (uint8 v1, bytes32 r1, bytes32 s1) = vm.sign(attestorPk, d1);
        vm.prank(alice);
        capped.mint(EVENT_ID, n1, abi.encodePacked(r1, s1, v1));

        bytes32 n2 = keccak256("c2");
        bytes32 d2 = capped.claimDigest(EVENT_ID, n2);
        (uint8 v2, bytes32 r2, bytes32 s2) = vm.sign(attestorPk, d2);
        vm.prank(bob);
        vm.expectRevert(FireflyPOAP.MaxSupplyReached.selector);
        capped.mint(EVENT_ID, n2, abi.encodePacked(r2, s2, v2));
    }

    function testMintToRecipient() public {
        // Relayer (test contract) submits; bob receives the POAP.
        bytes32 nonce = keccak256("relayed");
        bytes memory sig = _sign(attestorPk, EVENT_ID, nonce);

        uint256 tokenId = poap.mintTo(bob, EVENT_ID, nonce, sig);

        assertEq(poap.ownerOf(tokenId), bob);
        assertEq(poap.balanceOf(bob), 1);
        assertEq(poap.balanceOf(address(this)), 0);
    }

    function testSetAttestor() public {
        address newAttestor = address(0xCAFE);
        poap.setAttestor(newAttestor);
        assertEq(poap.attestor(), newAttestor);
    }

    // Cross-check fixture: a known (eventId, nonce) and its digest. The firmware
    // must produce the same digest for the same domain (chainId, verifyingContract).
    function testKnownDigestVector() public view {
        bytes32 nonce = 0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff;
        bytes32 digest = poap.claimDigest(EVENT_ID, nonce);
        // Recompute independently to guard against typehash/encoding drift.
        bytes32 structHash =
            keccak256(abi.encode(poap.CLAIM_TYPEHASH(), EVENT_ID, nonce));
        bytes32 expected = keccak256(
            abi.encodePacked(hex"1901", _domainSeparator(), structHash)
        );
        assertEq(digest, expected);
    }

    function _domainSeparator() internal view returns (bytes32) {
        return keccak256(
            abi.encode(
                keccak256(
                    "EIP712Domain(string name,string version,uint256 chainId,address verifyingContract)"
                ),
                keccak256(bytes("FireflyPOAP")),
                keccak256(bytes("1")),
                block.chainid,
                address(poap)
            )
        );
    }
}
