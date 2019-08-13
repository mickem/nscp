import React from 'react';
import { Provider } from 'react-redux'
import Main from './components/main';
import { store } from './helpers';

function App() {
  return (<Provider store={store}>
    <Main />
  </Provider>);
}

export default App;
