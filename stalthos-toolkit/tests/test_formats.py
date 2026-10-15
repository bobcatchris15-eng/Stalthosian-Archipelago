import pytest
from pathlib import Path
from stalthos_toolkit.schemas.formats import (
    LocatorInfo,
    classify_locator,
)

def test_classify_locator_empty():
    assert classify_locator("") == LocatorInfo("text", None, None)
    assert classify_locator("   \n") == LocatorInfo("text", None, None)

def test_classify_locator_doi():
    # bare DOIs
    assert classify_locator("10.1234/foo").type == "doi"
    assert classify_locator("10.1234/foo").normalized == "10.1234/foo"

    # prefixed DOIs
    assert classify_locator("doi:10.5678/bar").normalized == "10.5678/bar"
    assert classify_locator("doi: 10.5678/bar").normalized == "10.5678/bar"

    # url DOIs
    assert classify_locator("https://doi.org/10.9999/baz").normalized == "10.9999/baz"
    assert classify_locator("http://dx.doi.org/10.1111/qux").normalized == "10.1111/qux"

def test_classify_locator_url():
    assert classify_locator("http://example.com").type == "url"
    assert classify_locator("http://example.com").normalized == "http://example.com"
    assert classify_locator("https://foo.bar/baz").type == "url"
    assert classify_locator("HTTPS://FOO.BAR/baz").normalized == "https://FOO.BAR/baz"
    assert classify_locator("ftp://files.example.com").type == "url"

    assert classify_locator("custom://foo").type == "url"
    assert classify_locator("custom://foo").normalized == "custom://foo"

def test_classify_locator_identifier():
    assert classify_locator("urn:isbn:0451450523").type == "identifier"
    assert classify_locator("urn:isbn:0451450523").normalized == "urn:isbn:0451450523"

    assert classify_locator("taxon:1234").type == "identifier"
    assert classify_locator("TAXON:1234").normalized == "taxon:1234"

def test_classify_locator_local_path():
    assert classify_locator("/absolute/path.txt").type == "local_path"
    assert classify_locator("/absolute/path.txt").normalized == "/absolute/path.txt"
    assert classify_locator("relative/path.txt").type == "local_path"
    assert classify_locator("C:\\windows\\path").type == "local_path"
    assert classify_locator("file.txt").type == "local_path"

def test_classify_locator_text():
    assert classify_locator("Just some text").type == "text"
    assert classify_locator("Just some text").normalized == "Just some text"
    assert classify_locator("Book Title").type == "text"
