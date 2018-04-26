import { Injectable } from '@angular/core';
import { BaseService } from './base.service';
import { HttpClient, HttpResponse, HttpParams } from '@angular/common/http';
import { Observable } from 'rxjs';
import { LogRecord } from './logrecord';

@Injectable({
  providedIn: 'root'
})
export class LogService extends BaseService {

  constructor(http: HttpClient) {
    super("LogService", http);
  }

  public list(page: number, per_page: number): Observable<HttpResponse<LogRecord[]>> {
    return this.httpGet('/logs', { params: new HttpParams().set('page', ""+page).set('per_page', ""+per_page), observe: 'response' });
  }
  
}
