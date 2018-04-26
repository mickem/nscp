import { Injectable } from '@angular/core';
import { BaseService } from './base.service';
import { HttpClient, HttpResponse } from '@angular/common/http';
import { Observable } from 'rxjs';
import { Metric } from './metric';

@Injectable({
  providedIn: 'root'
})
export class MetricsService extends BaseService {

  constructor(http: HttpClient) {
    super("LogService", http);
  }

  public list(): Observable<Map<string, string>> {
    return this.httpGet('/metrics');
  }
}
