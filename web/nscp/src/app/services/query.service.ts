import { Injectable } from '@angular/core';
import { HttpClient, HttpHeaders } from '@angular/common/http';
import { Observable } from 'rxjs';





import { Query, QueryResponse } from './query';
import { BaseService } from './base.service';

@Injectable()
export class QueryService extends BaseService {

  constructor(http: HttpClient) {
    super("QueryService", http);
  }

  public list(): Observable<Query[]> {
    return this.httpGet('/queries');
  }
  public get(name: string): Observable<Query> {
    return this.httpGet('/queries/' + name + '/');
  }
  public exec(command: string): Observable<QueryResponse> {
    return this.httpGet('/queries/' + command + '/commands/execute');
  }

}
