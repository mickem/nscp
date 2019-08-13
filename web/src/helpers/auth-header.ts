export function authHeader() : HeadersInit {
    // return authorization header with jwt token
    let user = JSON.parse(localStorage.getItem('user') || '');

    if (user && user.token) {
        return { 'Authorization': 'Bearer ' + user.token };
    } else {
        return {};
    }
}