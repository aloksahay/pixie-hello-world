// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

import {ERC721} from "@openzeppelin/contracts/token/ERC721/ERC721.sol";
import {EIP712} from "@openzeppelin/contracts/utils/cryptography/EIP712.sol";
import {ECDSA} from "@openzeppelin/contracts/utils/cryptography/ECDSA.sol";
import {Ownable} from "@openzeppelin/contracts/access/Ownable.sol";
import {Strings} from "@openzeppelin/contracts/utils/Strings.sol";

/**
 * @title FireflyPOAP
 * @notice A POAP-style ERC-721 whose mints are gated by an attestation signed by
 *         a physical Firefly (Pixie) device. Only a claim carrying a valid
 *         secp256k1/EIP-712 signature from the trusted `attestor` key can mint.
 *
 *         Claims are bearer tokens (the Firefly cannot read the claimer's address
 *         without a camera/BLE): each `nonce` is single-use and the caller of
 *         {mint} receives the POAP. Supply is bounded here, not on the device:
 *         one POAP per wallet (unless `allowMultiplePerWallet`) and an optional
 *         `maxSupply`.
 */
contract FireflyPOAP is ERC721, EIP712, Ownable {
    using Strings for uint256;

    /// @dev keccak256("Claim(uint256 eventId,bytes32 nonce)")
    bytes32 public constant CLAIM_TYPEHASH =
        keccak256("Claim(uint256 eventId,bytes32 nonce)");

    /// @notice The Firefly device address authorized to attest mints.
    address public attestor;

    /// @notice The event this contract issues POAPs for.
    uint256 public immutable eventId;

    /// @notice 0 means unlimited.
    uint256 public maxSupply;

    /// @notice If false, an address may hold at most one POAP from this contract.
    bool public allowMultiplePerWallet;

    /// @notice Base URI for token metadata; tokenURI = baseArtURI + tokenId.
    string public baseArtURI;

    uint256 public nextTokenId;

    mapping(bytes32 => bool) public usedNonce;

    event AttestorUpdated(address indexed previous, address indexed current);
    event Claimed(address indexed to, uint256 indexed tokenId, bytes32 nonce);

    error BadAttestor();
    error NonceUsed();
    error WrongEvent();
    error AlreadyHolder();
    error MaxSupplyReached();

    constructor(
        string memory name_,
        string memory symbol_,
        address attestor_,
        uint256 eventId_,
        string memory baseArtURI_,
        uint256 maxSupply_,
        bool allowMultiplePerWallet_
    ) ERC721(name_, symbol_) EIP712("FireflyPOAP", "1") Ownable(msg.sender) {
        attestor = attestor_;
        eventId = eventId_;
        baseArtURI = baseArtURI_;
        maxSupply = maxSupply_;
        allowMultiplePerWallet = allowMultiplePerWallet_;
        emit AttestorUpdated(address(0), attestor_);
    }

    /**
     * @notice Compute the EIP-712 digest a Firefly must sign for `(eventId_, nonce)`.
     *         Exposed so the device/backend can be cross-checked byte-for-byte.
     */
    function claimDigest(uint256 eventId_, bytes32 nonce)
        public
        view
        returns (bytes32)
    {
        return _hashTypedDataV4(
            keccak256(abi.encode(CLAIM_TYPEHASH, eventId_, nonce))
        );
    }

    /**
     * @notice Recover the signer of a claim. View helper for off-chain checks.
     */
    function recoverClaim(uint256 eventId_, bytes32 nonce, bytes calldata signature)
        external
        view
        returns (address)
    {
        return ECDSA.recover(claimDigest(eventId_, nonce), signature);
    }

    /**
     * @notice Mint a POAP to the caller using a Firefly-signed claim.
     * @param eventId_  Must equal this contract's eventId.
     * @param nonce     Single-use nonce chosen by the device.
     * @param signature 65-byte secp256k1 signature (r‖s‖v, v in {27,28}).
     */
    function mint(uint256 eventId_, bytes32 nonce, bytes calldata signature)
        external
        returns (uint256 tokenId)
    {
        return _claim(msg.sender, eventId_, nonce, signature);
    }

    /**
     * @notice Mint a POAP to `to` using a Firefly-signed claim. Lets a relayer
     *         submit and pay gas on an attendee's behalf. Since claims are bearer
     *         tokens, the caller chooses the recipient; the attestation still
     *         gates whether any mint may happen at all.
     */
    function mintTo(
        address to,
        uint256 eventId_,
        bytes32 nonce,
        bytes calldata signature
    ) external returns (uint256 tokenId) {
        return _claim(to, eventId_, nonce, signature);
    }

    function _claim(
        address to,
        uint256 eventId_,
        bytes32 nonce,
        bytes calldata signature
    ) internal returns (uint256 tokenId) {
        if (eventId_ != eventId) revert WrongEvent();
        if (usedNonce[nonce]) revert NonceUsed();

        address signer = ECDSA.recover(claimDigest(eventId_, nonce), signature);
        if (signer != attestor) revert BadAttestor();

        if (!allowMultiplePerWallet && balanceOf(to) != 0) {
            revert AlreadyHolder();
        }
        if (maxSupply != 0 && nextTokenId >= maxSupply) revert MaxSupplyReached();

        usedNonce[nonce] = true;
        tokenId = nextTokenId++;
        _safeMint(to, tokenId);
        emit Claimed(to, tokenId, nonce);
    }

    /**
     * @notice Update the trusted Firefly address. In production this should only
     *         be set to an address whose genuine-Firefly RSA attestation
     *         (`attestProof`) has been verified off-chain.
     */
    function setAttestor(address attestor_) external onlyOwner {
        emit AttestorUpdated(attestor, attestor_);
        attestor = attestor_;
    }

    function setBaseArtURI(string calldata baseArtURI_) external onlyOwner {
        baseArtURI = baseArtURI_;
    }

    function tokenURI(uint256 tokenId)
        public
        view
        override
        returns (string memory)
    {
        _requireOwned(tokenId);
        return string.concat(baseArtURI, tokenId.toString());
    }
}
