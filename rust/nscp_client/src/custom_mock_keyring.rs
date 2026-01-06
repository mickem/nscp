use keyring::credential::{CredentialApi, CredentialBuilderApi, CredentialPersistence};
use keyring::error::decode_password;
use keyring::{Credential, CredentialBuilder, Error};
use std::cell::RefCell;
use std::collections::HashMap;
use std::sync::{LazyLock, Mutex};

static CREDENTIAL_STORE: LazyLock<Mutex<RefCell<HashMap<String, String>>>> =
    LazyLock::new(|| Mutex::new(RefCell::new(HashMap::new())));

fn set_cached_password(id: &str, password: &str) {
    if let Ok(store) = CREDENTIAL_STORE.lock() {
        let mut inner = store.borrow_mut();
        inner.insert(id.to_string(), password.to_string());
    }
}
fn get_cached_password(id: &str) -> Option<String> {
    if let Ok(store) = CREDENTIAL_STORE.lock() {
        let inner = store.borrow();
        inner.get(id).cloned()
    } else {
        None
    }
}

#[derive(Debug)]
pub struct MockCredential {
    pub id: String,
    pub inner: Mutex<RefCell<MockData>>,
}

#[derive(Debug, Default)]
pub struct MockData {
    pub secret: Option<Vec<u8>>,
    pub error: Option<Error>,
}

impl CredentialApi for MockCredential {
    fn set_password(&self, password: &str) -> keyring::Result<()> {
        let mut inner = self.inner.lock().expect("Can't access mock data for set");
        let data = inner.get_mut();
        let err = data.error.take();
        set_cached_password(&self.id, password);
        match err {
            None => {
                data.secret = Some(password.as_bytes().to_vec());
                Ok(())
            }
            Some(err) => Err(err),
        }
    }

    fn set_secret(&self, _secret: &[u8]) -> keyring::Result<()> {
        todo!()
    }

    fn get_password(&self) -> keyring::Result<String> {
        let mut inner = self.inner.lock().expect("Can't access mock data for get");
        let data = inner.get_mut();
        let err = data.error.take();
        match err {
            None => match &data.secret {
                None => Err(Error::NoEntry),
                Some(val) => decode_password(val.clone()),
            },
            Some(err) => Err(err),
        }
    }

    fn get_secret(&self) -> keyring::Result<Vec<u8>> {
        todo!()
    }

    fn delete_credential(&self) -> keyring::Result<()> {
        let mut inner = self
            .inner
            .lock()
            .expect("Can't access mock data for delete");
        let data = inner.get_mut();
        let err = data.error.take();
        match err {
            None => match data.secret {
                Some(_) => {
                    data.secret = None;
                    Ok(())
                }
                None => Err(Error::NoEntry),
            },
            Some(err) => Err(err),
        }
    }

    fn as_any(&self) -> &dyn std::any::Any {
        self
    }

    fn debug_fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        std::fmt::Debug::fmt(self, f)
    }
}

impl MockCredential {
    fn new_with_target(target: Option<&str>, service: &str, user: &str) -> keyring::Result<Self> {
        if let Some(pw) = get_cached_password(&format!(
            "MockCredential(target={:?}, service={}, user={})",
            target, service, user
        )) {
            let credential = Self {
                id: format!(
                    "MockCredential(target={:?}, service={}, user={})",
                    target, service, user
                ),
                inner: Mutex::new(RefCell::new(MockData {
                    secret: Some(pw.as_bytes().to_vec()),
                    error: None,
                })),
            };
            return Ok(credential);
        }
        Ok(Self {
            id: format!(
                "MockCredential(target={:?}, service={}, user={})",
                target, service, user
            ),
            inner: Mutex::new(RefCell::new(Default::default())),
        })
    }

    #[allow(dead_code)]
    pub fn set_error(&self, err: Error) {
        let mut inner = self
            .inner
            .lock()
            .expect("Can't access mock data for set_error");
        let data = inner.get_mut();
        data.error = Some(err);
    }
}

pub struct MockCredentialBuilder {}

impl CredentialBuilderApi for MockCredentialBuilder {
    fn build(
        &self,
        target: Option<&str>,
        service: &str,
        user: &str,
    ) -> keyring::Result<Box<Credential>> {
        let credential = MockCredential::new_with_target(target, service, user)?;
        Ok(Box::new(credential))
    }
    fn as_any(&self) -> &dyn std::any::Any {
        self
    }

    fn persistence(&self) -> CredentialPersistence {
        CredentialPersistence::EntryOnly
    }
}

pub fn default_credential_builder() -> Box<CredentialBuilder> {
    Box::new(MockCredentialBuilder {})
}
